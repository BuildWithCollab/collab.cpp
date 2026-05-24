#pragma once

#include <string>

namespace collab {

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

}  // namespace collab
