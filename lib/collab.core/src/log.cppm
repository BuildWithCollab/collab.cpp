module;

#include <filesystem>
#include <memory>
#include <string_view>
#include <utility>
#include <fmt/format.h>

export module collab.core:log;

import :manifest;

// MSVC C++20 modules quirk: including <fmt/format.h> in more than one partition's
// GMF causes ambiguous-specialization errors in any TU that imports collab.core
// AND directly includes spdlog (which itself pulls in fmt). To avoid that, every
// fmt-using template in this library lives in this partition.

export namespace collab::log {

enum class level { trace, debug, info, warn, error, critical, off };

class sink {
public:
    virtual ~sink() = default;
    virtual void write(level lvl,
                       const collab::core::identity* id,
                       std::string_view msg) = 0;
};

void set_level(level l);
level get_level();

void add_sink(std::unique_ptr<sink> s);
void clear_sinks();

void log_message(level lvl, const collab::core::identity* id, std::string_view msg);

std::unique_ptr<sink> make_stdout_sink();
std::unique_ptr<sink> make_stdout_color_sink();
std::unique_ptr<sink> make_stderr_sink();
std::unique_ptr<sink> make_stderr_color_sink();
std::unique_ptr<sink> make_file_sink(std::filesystem::path path);

// ── Untagged free functions: anonymous logging, no library attribution ───

// Plain string_view overloads
void trace(std::string_view msg);
void debug(std::string_view msg);
void info(std::string_view msg);
void warn(std::string_view msg);
void error(std::string_view msg);
void critical(std::string_view msg);

// fmt-style variadic overloads.
// The level check here avoids paying fmt::format cost for filtered messages.
// A TOCTOU exists (level could change between the check and log_message),
// but that's benign for logging — a message may slip through or be dropped
// during a concurrent level change, which is acceptable.
template<typename... Args>
void trace(fmt::format_string<Args...> fs, Args&&... args) {
    if (get_level() <= level::trace)
        trace(std::string_view(fmt::format(fs, std::forward<Args>(args)...)));
}

template<typename... Args>
void debug(fmt::format_string<Args...> fs, Args&&... args) {
    if (get_level() <= level::debug)
        debug(std::string_view(fmt::format(fs, std::forward<Args>(args)...)));
}

template<typename... Args>
void info(fmt::format_string<Args...> fs, Args&&... args) {
    if (get_level() <= level::info)
        info(std::string_view(fmt::format(fs, std::forward<Args>(args)...)));
}

template<typename... Args>
void warn(fmt::format_string<Args...> fs, Args&&... args) {
    if (get_level() <= level::warn)
        warn(std::string_view(fmt::format(fs, std::forward<Args>(args)...)));
}

template<typename... Args>
void error(fmt::format_string<Args...> fs, Args&&... args) {
    if (get_level() <= level::error)
        error(std::string_view(fmt::format(fs, std::forward<Args>(args)...)));
}

template<typename... Args>
void critical(fmt::format_string<Args...> fs, Args&&... args) {
    if (get_level() <= level::critical)
        critical(std::string_view(fmt::format(fs, std::forward<Args>(args)...)));
}

// ── Tagged free functions: message carries the caller's identity ─────────
// Sinks decide whether to render the tag, how to format it, and whether to
// filter on it.

void trace_with   (const collab::core::identity& id, std::string_view msg);
void debug_with   (const collab::core::identity& id, std::string_view msg);
void info_with    (const collab::core::identity& id, std::string_view msg);
void warn_with    (const collab::core::identity& id, std::string_view msg);
void error_with   (const collab::core::identity& id, std::string_view msg);
void critical_with(const collab::core::identity& id, std::string_view msg);

template<typename... Args>
void trace_with(const collab::core::identity& id, fmt::format_string<Args...> fs, Args&&... args) {
    if (get_level() <= level::trace)
        trace_with(id, std::string_view(fmt::format(fs, std::forward<Args>(args)...)));
}

template<typename... Args>
void debug_with(const collab::core::identity& id, fmt::format_string<Args...> fs, Args&&... args) {
    if (get_level() <= level::debug)
        debug_with(id, std::string_view(fmt::format(fs, std::forward<Args>(args)...)));
}

template<typename... Args>
void info_with(const collab::core::identity& id, fmt::format_string<Args...> fs, Args&&... args) {
    if (get_level() <= level::info)
        info_with(id, std::string_view(fmt::format(fs, std::forward<Args>(args)...)));
}

template<typename... Args>
void warn_with(const collab::core::identity& id, fmt::format_string<Args...> fs, Args&&... args) {
    if (get_level() <= level::warn)
        warn_with(id, std::string_view(fmt::format(fs, std::forward<Args>(args)...)));
}

template<typename... Args>
void error_with(const collab::core::identity& id, fmt::format_string<Args...> fs, Args&&... args) {
    if (get_level() <= level::error)
        error_with(id, std::string_view(fmt::format(fs, std::forward<Args>(args)...)));
}

template<typename... Args>
void critical_with(const collab::core::identity& id, fmt::format_string<Args...> fs, Args&&... args) {
    if (get_level() <= level::critical)
        critical_with(id, std::string_view(fmt::format(fs, std::forward<Args>(args)...)));
}

// ── Library-bound logger ────────────────────────────────────────────────
// Each library declares a single static `identity` (typically as a member of
// its manifest) and aliases the logger:
//
//   using log = collab::log::logger<collab::net::identity>;
//
// then calls `log::info("...")` throughout. All six methods are static —
// there is no per-call object construction. The identity is baked into the
// template instantiation as a reference NTTP.

template<const collab::core::identity& I>
struct logger {
    template<typename... Args>
    static void trace(fmt::format_string<Args...> fs, Args&&... args) {
        trace_with(I, fs, std::forward<Args>(args)...);
    }
    template<typename... Args>
    static void debug(fmt::format_string<Args...> fs, Args&&... args) {
        debug_with(I, fs, std::forward<Args>(args)...);
    }
    template<typename... Args>
    static void info(fmt::format_string<Args...> fs, Args&&... args) {
        info_with(I, fs, std::forward<Args>(args)...);
    }
    template<typename... Args>
    static void warn(fmt::format_string<Args...> fs, Args&&... args) {
        warn_with(I, fs, std::forward<Args>(args)...);
    }
    template<typename... Args>
    static void error(fmt::format_string<Args...> fs, Args&&... args) {
        error_with(I, fs, std::forward<Args>(args)...);
    }
    template<typename... Args>
    static void critical(fmt::format_string<Args...> fs, Args&&... args) {
        critical_with(I, fs, std::forward<Args>(args)...);
    }

    // Plain-string overloads for already-formatted messages.
    static void trace   (std::string_view msg) { trace_with   (I, msg); }
    static void debug   (std::string_view msg) { debug_with   (I, msg); }
    static void info    (std::string_view msg) { info_with    (I, msg); }
    static void warn    (std::string_view msg) { warn_with    (I, msg); }
    static void error   (std::string_view msg) { error_with   (I, msg); }
    static void critical(std::string_view msg) { critical_with(I, msg); }
};

}  // namespace collab::log
