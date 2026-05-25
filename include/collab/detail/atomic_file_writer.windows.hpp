#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <utility>

#include <collab/error.hpp>

// Forward-declared Win32 prototypes — no <windows.h>, no macro pollution.
// Calling convention and signatures match windows.h for these specific
// imports; the Win32 ABI is stable. Names match the kernel32 exports so
// the linker resolves them without an import library entry on our side.

extern "C" {

using _collab_BOOL    = int;
using _collab_DWORD   = unsigned long;
using _collab_LPCWSTR = const wchar_t*;
using _collab_HANDLE  = void*;

__declspec(dllimport) _collab_HANDLE __stdcall CreateFileW(
    _collab_LPCWSTR lpFileName,
    _collab_DWORD   dwDesiredAccess,
    _collab_DWORD   dwShareMode,
    void*           lpSecurityAttributes,
    _collab_DWORD   dwCreationDisposition,
    _collab_DWORD   dwFlagsAndAttributes,
    _collab_HANDLE  hTemplateFile);

__declspec(dllimport) _collab_BOOL __stdcall WriteFile(
    _collab_HANDLE hFile,
    const void*    lpBuffer,
    _collab_DWORD  nNumberOfBytesToWrite,
    _collab_DWORD* lpNumberOfBytesWritten,
    void*          lpOverlapped);

__declspec(dllimport) _collab_BOOL __stdcall FlushFileBuffers(_collab_HANDLE hFile);
__declspec(dllimport) _collab_BOOL __stdcall CloseHandle(_collab_HANDLE hObject);

__declspec(dllimport) _collab_BOOL __stdcall MoveFileExW(
    _collab_LPCWSTR lpExistingFileName,
    _collab_LPCWSTR lpNewFileName,
    _collab_DWORD   dwFlags);

__declspec(dllimport) _collab_DWORD __stdcall GetFileAttributesW(_collab_LPCWSTR lpFileName);
__declspec(dllimport) _collab_BOOL  __stdcall SetFileAttributesW(_collab_LPCWSTR lpFileName, _collab_DWORD dwFileAttributes);

__declspec(dllimport) _collab_DWORD __stdcall GetLastError(void);

__declspec(dllimport) _collab_DWORD __stdcall GetCurrentProcessId(void);

}  // extern "C"

namespace collab::detail {

inline constexpr _collab_DWORD _collab_GENERIC_WRITE             = 0x40000000;
inline constexpr _collab_DWORD _collab_FILE_SHARE_READ           = 0x00000001;
inline constexpr _collab_DWORD _collab_CREATE_ALWAYS             = 2;
inline constexpr _collab_DWORD _collab_FILE_ATTRIBUTE_NORMAL     = 0x00000080;
inline constexpr _collab_DWORD _collab_FILE_ATTRIBUTE_READONLY   = 0x00000001;
inline constexpr _collab_DWORD _collab_INVALID_FILE_ATTRIBUTES   = 0xFFFFFFFF;
inline constexpr _collab_DWORD _collab_MOVEFILE_REPLACE_EXISTING = 0x00000001;
inline constexpr _collab_DWORD _collab_MOVEFILE_WRITE_THROUGH    = 0x00000008;
inline constexpr _collab_DWORD _collab_ERROR_NOT_SAME_DEVICE     = 17;

inline _collab_HANDLE atomic_file_writer_handle_from_intptr(std::intptr_t h) {
    return reinterpret_cast<_collab_HANDLE>(h);
}

inline std::intptr_t atomic_file_writer_intptr_from_handle(_collab_HANDLE h) {
    return reinterpret_cast<std::intptr_t>(h);
}

inline void atomic_file_writer_check_target_writable(const std::filesystem::path& target) {
    const _collab_DWORD attrs = GetFileAttributesW(target.c_str());
    if (attrs == _collab_INVALID_FILE_ATTRIBUTES) {
        return;
    }
    if (attrs & _collab_FILE_ATTRIBUTE_READONLY) {
        throw collab::errors::atomic_file_write::target_read_only{target};
    }
}

inline std::filesystem::path atomic_file_writer_make_temp_path(const std::filesystem::path& target) {
    static std::atomic<std::uint64_t> counter{0};
    const auto                        n      = counter.fetch_add(1, std::memory_order_relaxed);
    const auto                        pid    = GetCurrentProcessId();
    const auto                        suffix = std::wstring{L"."} + std::to_wstring(pid) +
                                               L"." + std::to_wstring(n) + L".tmp";
    auto temp = target;
    temp += suffix;
    return temp;
}

inline std::intptr_t atomic_file_writer_open_temp(const std::filesystem::path& target,
                                                  const std::filesystem::path& temp) {
    const _collab_HANDLE h = CreateFileW(
        temp.c_str(),
        _collab_GENERIC_WRITE,
        _collab_FILE_SHARE_READ,
        nullptr,
        _collab_CREATE_ALWAYS,
        _collab_FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (atomic_file_writer_intptr_from_handle(h) == atomic_file_writer_invalid_handle) {
        throw collab::errors::atomic_file_write::create_temp_failed{
            target, static_cast<int>(GetLastError())};
    }
    return atomic_file_writer_intptr_from_handle(h);
}

inline void atomic_file_writer_close(std::intptr_t handle) {
    if (handle != atomic_file_writer_invalid_handle) {
        CloseHandle(atomic_file_writer_handle_from_intptr(handle));
    }
}

inline void atomic_file_writer_remove_quietly(const std::filesystem::path& temp) {
    std::error_code ec;
    std::filesystem::remove(temp, ec);
}

inline void atomic_file_writer_write(std::intptr_t handle,
                                     const std::filesystem::path& target,
                                     std::span<const std::byte>   bytes) {
    const std::byte* p         = bytes.data();
    std::size_t      remaining = bytes.size();
    while (remaining > 0) {
        const _collab_DWORD chunk =
            remaining > 0x40000000u ? 0x40000000u : static_cast<_collab_DWORD>(remaining);
        _collab_DWORD written = 0;
        if (!WriteFile(atomic_file_writer_handle_from_intptr(handle),
                       p, chunk, &written, nullptr) ||
            written == 0)
        {
            throw collab::errors::atomic_file_write::write_failed{
                target, static_cast<int>(GetLastError())};
        }
        p         += written;
        remaining -= written;
    }
}

inline void atomic_file_writer_fsync(std::intptr_t handle,
                                     const std::filesystem::path& target) {
    if (!FlushFileBuffers(atomic_file_writer_handle_from_intptr(handle))) {
        throw collab::errors::atomic_file_write::fsync_temp_failed{
            target, static_cast<int>(GetLastError())};
    }
}

inline void atomic_file_writer_preserve_attributes(const std::filesystem::path& target,
                                                   const std::filesystem::path& temp) {
    const _collab_DWORD existing = GetFileAttributesW(target.c_str());
    if (existing == _collab_INVALID_FILE_ATTRIBUTES) {
        return;
    }
    if (!SetFileAttributesW(temp.c_str(), existing)) {
        throw collab::errors::atomic_file_write::permission_copy_failed{
            target, static_cast<int>(GetLastError())};
    }
}

inline void atomic_file_writer_atomic_replace(const std::filesystem::path& temp,
                                              const std::filesystem::path& target,
                                              bool                         fallback_enabled) {
    if (MoveFileExW(temp.c_str(), target.c_str(),
                    _collab_MOVEFILE_REPLACE_EXISTING | _collab_MOVEFILE_WRITE_THROUGH))
    {
        return;
    }
    const _collab_DWORD err = GetLastError();
    if (err == _collab_ERROR_NOT_SAME_DEVICE) {
        if (!fallback_enabled) {
            throw collab::errors::atomic_file_write::cross_filesystem{
                target, static_cast<int>(err)};
        }
        std::error_code ec;
        std::filesystem::copy_file(
            temp, target, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            throw collab::errors::atomic_file_write::direct_write_failed{
                target, ec.value()};
        }
        std::filesystem::remove(temp, ec);
        return;
    }
    throw collab::errors::atomic_file_write::rename_failed{
        target, static_cast<int>(err)};
}

inline void atomic_file_writer_fsync_parent_dir(const std::filesystem::path& /*target*/) {
    // No-op on Windows: MOVEFILE_WRITE_THROUGH on MoveFileExW already flushes
    // the directory metadata to disk before returning.
}

}  // namespace collab::detail
