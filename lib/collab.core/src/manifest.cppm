module;

#include <optional>
#include <string>
#include <vector>

export module collab.core:manifest;

import :semver;

export namespace collab::core {

// Identity bits: the parts used to derive paths, bundle IDs, folder names.
// Kept separate so callers that only need identity (e.g. config path resolution)
// don't have to drag descriptive metadata around.
struct identity {
    std::string app_id;    // app slug,            e.g. "collab-core"
    std::string app_name;  // app display name,    e.g. "Collab Core"
    std::string org_id;    // org rDNS slug,       e.g. "mrowrpurr"
    std::string org_name;  // org display name,    e.g. "Mrowr Purr"
    std::string tld;       // rDNS root segment,   e.g. "com"

    // Reverse-DNS bundle ID: "tld.org_id.app_id" (e.g. "com.mrowrpurr.collab-core").
    std::string bundle_id() const;
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
