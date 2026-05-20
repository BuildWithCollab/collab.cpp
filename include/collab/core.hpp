// collab-core single-header API.
//
// This header is the canonical entry point for header-only consumers. The
// inline definitions below are attached to the global module (whether reached
// via #include in a non-module TU or via #include in a module's global
// module fragment).
//
// The modular library (`import collab.core;`) does NOT GMF-include this
// header; instead its primary interface unit GMF-includes just the small
// "collab/detail/types.hpp" header (which holds the value types + `level` +
// `sink`) and re-exports those names via using-declarations, then declares
// its own module-attached log API. Doing it that way keeps the *types*
// single-sourced across both consumption modes while sidestepping the
// cross-attachment redeclaration of the inline log functions.
//
// The inline template definitions and the logger<I> class template are
// shared between this header and the module via "collab/detail/log_inline.hpp"
// — that file is the single source of truth for those bodies.
//
// Header-only consumers need `<fmt/format.h>` reachable in their build (either
// linked or with FMT_HEADER_ONLY defined). No link contract from collab.core
// itself — the spdlog-backed sink factories (`make_*_sink`) live only in the
// modular library and aren't declared here.
//
// The state singleton in <collab/detail/state.hpp> is shared between the
// inline functions below and the module impl unit, so sinks, level, and the
// dispatch mutex are unique across the program regardless of which entry
// path any given TU used.

#pragma once

#include <memory>
#include <string_view>
#include <utility>

#include <fmt/format.h>

#include "collab/detail/state.hpp"
#include "collab/detail/types.hpp"

namespace collab::log {

// ── State management ────────────────────────────────────────────────────

inline void set_level(level l) {
    auto& s = detail::state();
    std::lock_guard lock(s.mtx);
    s.current_level = l;
}

inline level get_level() {
    auto& s = detail::state();
    std::lock_guard lock(s.mtx);
    return s.current_level;
}

inline void add_sink(std::unique_ptr<sink> snk) {
    auto& s = detail::state();
    std::lock_guard lock(s.mtx);
    s.sinks.push_back(std::move(snk));
}

inline void clear_sinks() {
    auto& s = detail::state();
    std::lock_guard lock(s.mtx);
    s.sinks.clear();
}

inline void log_message(level lvl, const collab::core::identifier* id, std::string_view msg) {
    auto& s = detail::state();
    std::lock_guard lock(s.mtx);
    if (lvl < s.current_level) return;
    for (auto& snk : s.sinks)
        snk->write(lvl, id, msg);
}

// ── Untagged free functions: anonymous logging, no library attribution ──

inline void trace   (std::string_view msg) { log_message(level::trace,    nullptr, msg); }
inline void debug   (std::string_view msg) { log_message(level::debug,    nullptr, msg); }
inline void info    (std::string_view msg) { log_message(level::info,     nullptr, msg); }
inline void warn    (std::string_view msg) { log_message(level::warn,     nullptr, msg); }
inline void error   (std::string_view msg) { log_message(level::error,    nullptr, msg); }
inline void critical(std::string_view msg) { log_message(level::critical, nullptr, msg); }

// ── Tagged free functions: message carries the caller's identifier ────────

inline void trace_with(const collab::core::identifier& id, std::string_view msg) {
    log_message(level::trace, &id, msg);
}
inline void debug_with(const collab::core::identifier& id, std::string_view msg) {
    log_message(level::debug, &id, msg);
}
inline void info_with(const collab::core::identifier& id, std::string_view msg) {
    log_message(level::info, &id, msg);
}
inline void warn_with(const collab::core::identifier& id, std::string_view msg) {
    log_message(level::warn, &id, msg);
}
inline void error_with(const collab::core::identifier& id, std::string_view msg) {
    log_message(level::error, &id, msg);
}
inline void critical_with(const collab::core::identifier& id, std::string_view msg) {
    log_message(level::critical, &id, msg);
}

}  // namespace collab::log

// ── fmt-style variadic templates + logger<I> class template ─────────────
// Single source of truth — same definitions reached via the module path.

#include "collab/detail/log_inline.hpp"
