#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

#if defined(_WIN32)
extern "C" __declspec(dllimport) unsigned long __stdcall GetCurrentProcessId(void);
#else
#  include <unistd.h>
#endif

namespace collab::test_helpers {

// RAII unique temp directory under std::filesystem::temp_directory_path().
// pid + steady-clock nanos + atomic counter + tag → uniqueness across parallel
// processes, parallel threads, and same-thread reuse. dtor uses the
// std::error_code overload of remove_all so it never throws.
struct temp_dir {
    std::filesystem::path path;

    explicit temp_dir(std::string_view tag = "test") {
        static std::atomic<std::uint64_t> counter{0};
        const auto pid   =
#if defined(_WIN32)
            static_cast<std::uint64_t>(::GetCurrentProcessId());
#else
            static_cast<std::uint64_t>(::getpid());
#endif
        const auto nanos = static_cast<std::uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
        const auto n     = counter.fetch_add(1, std::memory_order_relaxed);
        std::string name = "collab-core-";
        name.append(tag);
        name += '-';
        name += std::to_string(pid);
        name += '-';
        name += std::to_string(nanos);
        name += '-';
        name += std::to_string(n);
        path = std::filesystem::temp_directory_path() / name;
        std::filesystem::create_directories(path);
    }

    ~temp_dir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    temp_dir(const temp_dir&)            = delete;
    temp_dir& operator=(const temp_dir&) = delete;
    temp_dir(temp_dir&&)                 = delete;
    temp_dir& operator=(temp_dir&&)      = delete;
};

}  // namespace collab::test_helpers
