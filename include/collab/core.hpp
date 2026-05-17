// collab-core single-header API.
//
// This header is the canonical source for the value types and the public
// logging API. The modular library (`import collab.core;`) GMF-includes this
// same header in its partition interface units and re-exports the names —
// downstream sees identical declarations either way.
//
// Header-only consumers need `<fmt/format.h>` reachable in their build (either
// linked or with FMT_HEADER_ONLY defined). No link contract from collab.core
// itself — the spdlog-backed sink factories (`make_*_sink`) live only in the
// modular library and aren't declared here.
//
// All inline definitions in this file are attached to the global module
// (whether reached via #include in a non-module TU or via #include in a
// module's global module fragment), so the state singleton, sinks vector,
// and level are shared across module and non-module consumers in the same
// binary.

#pragma once

#include <compare>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <fmt/format.h>

// ══════════════════════════════════════════════════════════════════════════
// Value types
// ══════════════════════════════════════════════════════════════════════════

namespace collab::core {

// Semantic version per SemVer 2.0 (https://semver.org).
struct semver {
    int         major = 0;
    int         minor = 0;
    int         patch = 0;
    std::string pre_release;
    std::string build;

    inline std::string to_string() const;

    // Both hand-written: defaulting either would include `build`, which
    // SemVer §10 requires to be ignored when determining precedence.
    inline std::strong_ordering operator<=>(const semver&) const;
    inline bool                 operator==(const semver&) const;
};

// Identity bits used to derive paths, bundle IDs, folder names.
struct identity {
    std::string app_id;    // app slug,            e.g. "collab-core"
    std::string app_name;  // app display name,    e.g. "Collab Core"
    std::string org_id;    // org rDNS slug,       e.g. "mrowrpurr"
    std::string org_name;  // org display name,    e.g. "Mrowr Purr"
    std::string tld;       // rDNS root segment,   e.g. "com"

    // Reverse-DNS bundle ID: "tld.org_id.app_id".
    inline std::string bundle_id() const;
};

// Project manifest: identity plus descriptive metadata.
struct manifest {
    core::identity             identity;
    semver                     version;
    std::optional<std::string> description;
    std::vector<std::string>   authors;
    std::optional<std::string> license;  // SPDX identifier, e.g. "0BSD"
};

}  // namespace collab::core

// ══════════════════════════════════════════════════════════════════════════
// Value type implementations (inline)
// ══════════════════════════════════════════════════════════════════════════

namespace collab::core {

namespace detail {

    inline std::vector<std::string_view> split_identifiers(std::string_view s) {
        std::vector<std::string_view> out;
        std::size_t                   start = 0;
        for (std::size_t i = 0; i <= s.size(); ++i) {
            if (i == s.size() || s[i] == '.') {
                out.emplace_back(s.substr(start, i - start));
                start = i + 1;
            }
        }
        return out;
    }

    inline bool is_numeric_identifier(std::string_view id) {
        if (id.empty()) return false;
        for (char c : id) {
            if (c < '0' || c > '9') return false;
        }
        return true;
    }

    inline unsigned long long parse_numeric(std::string_view id) {
        unsigned long long result = 0;
        for (char c : id) result = result * 10 + static_cast<unsigned long long>(c - '0');
        return result;
    }

    // Pre-release precedence per SemVer §11. Caller guarantees both are non-empty.
    inline std::strong_ordering compare_pre_release(std::string_view a, std::string_view b) {
        auto ids_a = split_identifiers(a);
        auto ids_b = split_identifiers(b);

        const std::size_t n = std::min(ids_a.size(), ids_b.size());
        for (std::size_t i = 0; i < n; ++i) {
            const auto xa = ids_a[i];
            const auto xb = ids_b[i];
            const bool na = is_numeric_identifier(xa);
            const bool nb = is_numeric_identifier(xb);

            if (na && nb) {
                const auto va = parse_numeric(xa);
                const auto vb = parse_numeric(xb);
                if (auto cmp = va <=> vb; cmp != 0) return cmp;
            } else if (na && !nb) {
                return std::strong_ordering::less;
            } else if (!na && nb) {
                return std::strong_ordering::greater;
            } else {
                if (const auto cmp = xa.compare(xb); cmp != 0) {
                    return cmp < 0 ? std::strong_ordering::less : std::strong_ordering::greater;
                }
            }
        }

        if (ids_a.size() < ids_b.size()) return std::strong_ordering::less;
        if (ids_a.size() > ids_b.size()) return std::strong_ordering::greater;
        return std::strong_ordering::equal;
    }

}  // namespace detail

inline std::string semver::to_string() const {
    std::string out;
    out.reserve(16);
    out += std::to_string(major);
    out += '.';
    out += std::to_string(minor);
    out += '.';
    out += std::to_string(patch);
    if (!pre_release.empty()) {
        out += '-';
        out += pre_release;
    }
    if (!build.empty()) {
        out += '+';
        out += build;
    }
    return out;
}

inline std::strong_ordering semver::operator<=>(const semver& other) const {
    if (const auto cmp = major <=> other.major; cmp != 0) return cmp;
    if (const auto cmp = minor <=> other.minor; cmp != 0) return cmp;
    if (const auto cmp = patch <=> other.patch; cmp != 0) return cmp;

    const bool a_has = !pre_release.empty();
    const bool b_has = !other.pre_release.empty();
    if (!a_has && !b_has) return std::strong_ordering::equal;
    if (!a_has && b_has)  return std::strong_ordering::greater;
    if (a_has  && !b_has) return std::strong_ordering::less;
    return detail::compare_pre_release(pre_release, other.pre_release);
}

inline bool semver::operator==(const semver& other) const {
    return major == other.major
        && minor == other.minor
        && patch == other.patch
        && pre_release == other.pre_release;
}

inline std::string identity::bundle_id() const {
    std::string out;
    out.reserve(tld.size() + org_id.size() + app_id.size() + 2);
    out += tld;
    out += '.';
    out += org_id;
    out += '.';
    out += app_id;
    return out;
}

}  // namespace collab::core

// ══════════════════════════════════════════════════════════════════════════
// Logging
// ══════════════════════════════════════════════════════════════════════════

namespace collab::log {

enum class level { trace, debug, info, warn, error, critical, off };

class sink {
public:
    virtual ~sink() = default;
    virtual void write(level lvl,
                       const collab::core::identity* id,
                       std::string_view msg) = 0;
};

namespace detail {

    struct log_state {
        level                              current_level = level::info;
        std::vector<std::unique_ptr<sink>> sinks;
        std::mutex                         mtx;
    };

    inline log_state& state() {
        static log_state s;
        return s;
    }

}  // namespace detail

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

inline void log_message(level lvl, const collab::core::identity* id, std::string_view msg) {
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

// ── Tagged free functions: message carries the caller's identity ────────

inline void trace_with(const collab::core::identity& id, std::string_view msg) {
    log_message(level::trace, &id, msg);
}
inline void debug_with(const collab::core::identity& id, std::string_view msg) {
    log_message(level::debug, &id, msg);
}
inline void info_with(const collab::core::identity& id, std::string_view msg) {
    log_message(level::info, &id, msg);
}
inline void warn_with(const collab::core::identity& id, std::string_view msg) {
    log_message(level::warn, &id, msg);
}
inline void error_with(const collab::core::identity& id, std::string_view msg) {
    log_message(level::error, &id, msg);
}
inline void critical_with(const collab::core::identity& id, std::string_view msg) {
    log_message(level::critical, &id, msg);
}

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
//   using log = collab::log::logger<my_lib::identity>;
//
// then calls `log::info("...")` throughout. All six methods are static —
// no per-call object construction. The identity is baked into the template
// instantiation as a reference NTTP.

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

    static void trace   (std::string_view msg) { trace_with   (I, msg); }
    static void debug   (std::string_view msg) { debug_with   (I, msg); }
    static void info    (std::string_view msg) { info_with    (I, msg); }
    static void warn    (std::string_view msg) { warn_with    (I, msg); }
    static void error   (std::string_view msg) { error_with   (I, msg); }
    static void critical(std::string_view msg) { critical_with(I, msg); }
};

}  // namespace collab::log
