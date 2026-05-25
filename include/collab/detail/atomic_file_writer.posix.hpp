#pragma once

#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <span>
#include <string>
#include <utility>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <collab/error.hpp>

namespace collab::detail {

inline void atomic_file_writer_check_target_writable(const std::filesystem::path& target) {
    struct ::stat st{};
    if (::stat(target.c_str(), &st) != 0) {
        return;
    }
    if ((st.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH)) == 0) {
        throw collab::errors::atomic_file_write::target_read_only{target};
    }
}

inline std::filesystem::path atomic_file_writer_make_temp_path(const std::filesystem::path& target) {
    static std::atomic<std::uint64_t> counter{0};
    const auto                        n      = counter.fetch_add(1, std::memory_order_relaxed);
    const auto                        pid    = static_cast<std::uint64_t>(::getpid());
    auto                              temp   = target;
    temp += "." + std::to_string(pid) + "." + std::to_string(n) + ".tmp";
    return temp;
}

inline std::intptr_t atomic_file_writer_open_temp(const std::filesystem::path& target,
                                                  const std::filesystem::path& temp) {
    const int fd = ::open(temp.c_str(),
                          O_WRONLY | O_CREAT | O_TRUNC,
                          S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0) {
        throw collab::errors::atomic_file_write::create_temp_failed{target, errno};
    }
    return static_cast<std::intptr_t>(fd);
}

inline void atomic_file_writer_close(std::intptr_t handle) {
    if (handle != atomic_file_writer_invalid_handle) {
        ::close(static_cast<int>(handle));
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
    const int        fd        = static_cast<int>(handle);
    while (remaining > 0) {
        const ::ssize_t n = ::write(fd, p, remaining);
        if (n < 0) {
            if (errno == EINTR) continue;
            throw collab::errors::atomic_file_write::write_failed{target, errno};
        }
        if (n == 0) {
            throw collab::errors::atomic_file_write::write_failed{target, 0};
        }
        p         += n;
        remaining -= static_cast<std::size_t>(n);
    }
}

inline void atomic_file_writer_fsync(std::intptr_t handle,
                                     const std::filesystem::path& target) {
    const int fd = static_cast<int>(handle);
    if (::fsync(fd) != 0) {
        throw collab::errors::atomic_file_write::fsync_temp_failed{target, errno};
    }
}

inline void atomic_file_writer_preserve_attributes(const std::filesystem::path& target,
                                                   const std::filesystem::path& temp) {
    struct ::stat st{};
    if (::stat(target.c_str(), &st) != 0) {
        return;
    }
    if (::chmod(temp.c_str(), st.st_mode & 07777) != 0) {
        throw collab::errors::atomic_file_write::permission_copy_failed{target, errno};
    }
    // chown is best-effort: requires CAP_CHOWN on Linux and is commonly
    // refused for non-root processes. Silently ignore EPERM so unprivileged
    // saves don't blow up; the mode bits are the meaningful part for config files.
    if (::chown(temp.c_str(), st.st_uid, st.st_gid) != 0 && errno != EPERM) {
        throw collab::errors::atomic_file_write::permission_copy_failed{target, errno};
    }
}

inline void atomic_file_writer_atomic_replace(const std::filesystem::path& temp,
                                              const std::filesystem::path& target,
                                              bool                         fallback_enabled) {
    if (::rename(temp.c_str(), target.c_str()) == 0) {
        return;
    }
    const int err = errno;
    if (err == EXDEV) {
        if (!fallback_enabled) {
            throw collab::errors::atomic_file_write::cross_filesystem{target, err};
        }
        std::error_code ec;
        std::filesystem::copy_file(
            temp, target, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            throw collab::errors::atomic_file_write::direct_write_failed{target, ec.value()};
        }
        std::filesystem::remove(temp, ec);
        return;
    }
    throw collab::errors::atomic_file_write::rename_failed{target, err};
}

inline void atomic_file_writer_fsync_parent_dir(const std::filesystem::path& target) {
    const auto parent = target.parent_path().empty()
                            ? std::filesystem::path{"."}
                            : target.parent_path();
    const int  dirfd  = ::open(parent.c_str(), O_RDONLY
#ifdef O_DIRECTORY
                              | O_DIRECTORY
#endif
                              );
    if (dirfd < 0) {
        throw collab::errors::atomic_file_write::fsync_parent_dir_failed{target, errno};
    }
    if (::fsync(dirfd) != 0) {
        const int err = errno;
        ::close(dirfd);
        // Some filesystems (e.g. tmpfs on some kernels) return EINVAL for
        // fsync on a directory fd. Treat that as "best effort done."
        if (err == EINVAL) return;
        throw collab::errors::atomic_file_write::fsync_parent_dir_failed{target, err};
    }
    ::close(dirfd);
}

}  // namespace collab::detail
