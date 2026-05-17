module;

#include <string>

module collab.core;

import :manifest;

namespace collab::core {

std::string identity::bundle_id() const {
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
