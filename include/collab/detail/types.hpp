// Value types + log enum + sink interface for collab.core.
//
// Split out of <collab/core.hpp> so the module's primary interface unit can
// GMF-include just these types (and re-export them via using-declarations)
// without dragging in the inline log API definitions. Including the full
// <collab/core.hpp> in the module's GMF would create cross-attachment
// redeclarations of the log free functions ([basic.link]/9 ill-formed).
//
// Header-only consumers reach these names through <collab/core.hpp>, which
// includes this header.

#pragma once

#include <compare>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

// Identifier bits used to derive paths, bundle IDs, folder names.
struct identifier {
    std::string app_id;    // app slug,            e.g. "collab-core"
    std::string app_name;  // app display name,    e.g. "Collab Core"
    std::string org_id;    // org rDNS slug,       e.g. "mrowrpurr"
    std::string org_name;  // org display name,    e.g. "Mrowr Purr"
    std::string tld;       // rDNS root segment,   e.g. "com"

    // Reverse-DNS bundle ID: "tld.org_id.app_id".
    inline std::string bundle_id() const;
};

// Project manifest: identifier plus descriptive metadata.
struct manifest {
    core::identifier             identifier;
    semver                     version;
    std::optional<std::string> description;
    std::vector<std::string>   authors;
    std::optional<std::string> license;  // SPDX identifier, e.g. "0BSD"
};

// Library version. `inline const` (not constexpr): semver carries
// std::string fields, which make it non-literal under libstdc++. MSVC
// accepts constexpr; GCC rejects it. Inline const is enough for the
// single-definition guarantee across consumers.
inline const semver version{1, 0, 0};

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

inline std::string identifier::bundle_id() const {
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
// Log enum + sink interface
// ══════════════════════════════════════════════════════════════════════════

namespace collab::log {

enum class level { trace, debug, info, warn, error, critical, off };

class sink {
public:
    virtual ~sink() = default;
    virtual void write(level lvl,
                       const collab::core::identifier* id,
                       std::string_view msg) = 0;
};

}  // namespace collab::log
