// Inline template definitions for the collab log API and the logger<I>
// class template. Included by:
//
//   - <collab/core.hpp>          (header-only consumers — entities attach to
//                                 the global module)
//   - lib/collab.core/src/
//     collab.core.cppm purview   (module consumers — entities attach to the
//                                 named module collab.core)
//
// In both contexts the template bodies are identical, and in both contexts
// the non-template overloads they recurse into (`trace(std::string_view)`,
// etc.) are already declared/defined in the enclosing namespace before this
// header is included, so unqualified name lookup resolves correctly.
//
// Wrap the include in `export { ... }` from the module side to mark the
// templates exported; from the header-only side just include it normally.

// IMPORTANT: this header has no #include directives by design. It is included
// from both <collab/core.hpp> (header-only path, attached to global module)
// and from lib/collab.core/src/collab.core.cppm's purview (module path,
// attached to module collab.core). Putting #include directives here would
// drag <fmt/format.h>, <string_view>, etc. into module attachment when the
// cppm includes this file in its purview — MSVC rejects that, and other
// compilers should too. Each consumer must arrange for `fmt::format_string`,
// `std::string_view`, `std::forward`, and the collab::log non-template
// declarations to be visible before including this file.

#pragma once

namespace collab::log {

// fmt-style variadic overloads. The level check here avoids paying fmt::format
// cost for filtered messages. A TOCTOU exists (level could change between the
// check and log_message), but that's benign for logging — a message may slip
// through or be dropped during a concurrent level change, which is acceptable.

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

template<typename... Args>
void trace_with(const collab::core::identifier& id, fmt::format_string<Args...> fs, Args&&... args) {
    if (get_level() <= level::trace)
        trace_with(id, std::string_view(fmt::format(fs, std::forward<Args>(args)...)));
}

template<typename... Args>
void debug_with(const collab::core::identifier& id, fmt::format_string<Args...> fs, Args&&... args) {
    if (get_level() <= level::debug)
        debug_with(id, std::string_view(fmt::format(fs, std::forward<Args>(args)...)));
}

template<typename... Args>
void info_with(const collab::core::identifier& id, fmt::format_string<Args...> fs, Args&&... args) {
    if (get_level() <= level::info)
        info_with(id, std::string_view(fmt::format(fs, std::forward<Args>(args)...)));
}

template<typename... Args>
void warn_with(const collab::core::identifier& id, fmt::format_string<Args...> fs, Args&&... args) {
    if (get_level() <= level::warn)
        warn_with(id, std::string_view(fmt::format(fs, std::forward<Args>(args)...)));
}

template<typename... Args>
void error_with(const collab::core::identifier& id, fmt::format_string<Args...> fs, Args&&... args) {
    if (get_level() <= level::error)
        error_with(id, std::string_view(fmt::format(fs, std::forward<Args>(args)...)));
}

template<typename... Args>
void critical_with(const collab::core::identifier& id, fmt::format_string<Args...> fs, Args&&... args) {
    if (get_level() <= level::critical)
        critical_with(id, std::string_view(fmt::format(fs, std::forward<Args>(args)...)));
}

// ── Library-bound logger ────────────────────────────────────────────────
// Each library declares a single static `identifier` (typically as a member of
// its manifest) and aliases the logger:
//
//   using log = collab::log::logger<my_lib::identifier>;
//
// then calls `log::info("...")` throughout. All six methods are static —
// no per-call object construction. The identifier is baked into the template
// instantiation as a reference NTTP.

template<const collab::core::identifier& I>
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

    static void trace   (std::string_view msg) { trace_with   (I, msg); }
    static void debug   (std::string_view msg) { debug_with   (I, msg); }
    static void info    (std::string_view msg) { info_with    (I, msg); }
    static void warn    (std::string_view msg) { warn_with    (I, msg); }
    static void error   (std::string_view msg) { error_with   (I, msg); }
    static void critical(std::string_view msg) { critical_with(I, msg); }
};

}  // namespace collab::log
