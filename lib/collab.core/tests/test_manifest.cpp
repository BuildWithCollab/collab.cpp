#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

import collab.core;

using collab::core::identity;
using collab::core::manifest;
using collab::core::semver;

TEST_CASE("identity construction", "[manifest][identity]") {
    const identity ident{
        .app_id   = "collab-core",
        .app_name = "Collab Core",
        .org_id   = "mrowrpurr",
        .org_name = "Mrowr Purr",
        .tld      = "com",
    };
    REQUIRE(ident.app_id   == "collab-core");
    REQUIRE(ident.app_name == "Collab Core");
    REQUIRE(ident.org_id   == "mrowrpurr");
    REQUIRE(ident.org_name == "Mrowr Purr");
    REQUIRE(ident.tld      == "com");
}

TEST_CASE("identity default fields are empty", "[manifest][identity]") {
    const identity ident{};
    REQUIRE(ident.app_id.empty());
    REQUIRE(ident.app_name.empty());
    REQUIRE(ident.org_id.empty());
    REQUIRE(ident.org_name.empty());
    REQUIRE(ident.tld.empty());
}

TEST_CASE("identity::bundle_id formats as tld.org_id.app_id", "[manifest][identity][bundle_id]") {
    const identity ident{
        .app_id   = "collab-core",
        .app_name = "Collab Core",
        .org_id   = "mrowrpurr",
        .org_name = "Mrowr Purr",
        .tld      = "com",
    };
    REQUIRE(ident.bundle_id() == "com.mrowrpurr.collab-core");
}

TEST_CASE("identity::bundle_id ignores display names", "[manifest][identity][bundle_id]") {
    const identity ident{
        .app_id   = "my-tool",
        .app_name = "Completely Different Display Name",
        .org_id   = "acme",
        .org_name = "ACME Corporation, LLC",
        .tld      = "io",
    };
    REQUIRE(ident.bundle_id() == "io.acme.my-tool");
}

TEST_CASE("manifest construction with only required fields", "[manifest]") {
    const manifest m{
        .identity = {
            .app_id   = "thing",
            .app_name = "Thing",
            .org_id   = "purr",
            .org_name = "Purr",
            .tld      = "com",
        },
        .version = semver{0, 1, 0},
    };
    REQUIRE(m.identity.bundle_id() == "com.purr.thing");
    REQUIRE(m.version == semver{0, 1, 0});
    REQUIRE_FALSE(m.description.has_value());
    REQUIRE(m.authors.empty());
    REQUIRE_FALSE(m.license.has_value());
}

TEST_CASE("manifest construction with full metadata", "[manifest]") {
    const manifest m{
        .identity = {
            .app_id   = "collab-core",
            .app_name = "Collab Core",
            .org_id   = "mrowrpurr",
            .org_name = "Mrowr Purr",
            .tld      = "com",
        },
        .version     = semver{1, 2, 0, "rc.1"},
        .description = "Foundational C++23 library for the Collab stack.",
        .authors     = {"Mrowr Purr", "Parker"},
        .license     = "0BSD",
    };
    REQUIRE(m.identity.bundle_id() == "com.mrowrpurr.collab-core");
    REQUIRE(m.version.to_string() == "1.2.0-rc.1");
    REQUIRE(m.description.has_value());
    REQUIRE(*m.description == "Foundational C++23 library for the Collab stack.");
    REQUIRE(m.authors.size() == 2);
    REQUIRE(m.authors[0] == "Mrowr Purr");
    REQUIRE(m.authors[1] == "Parker");
    REQUIRE(m.license.has_value());
    REQUIRE(*m.license == "0BSD");
}

TEST_CASE("manifest distinguishes unset from explicitly empty", "[manifest][optional]") {
    manifest m{
        .identity = {
            .app_id   = "x",
            .app_name = "X",
            .org_id   = "o",
            .org_name = "O",
            .tld      = "t",
        },
        .version = semver{0, 0, 1},
    };
    REQUIRE_FALSE(m.license.has_value());

    m.license = "";
    REQUIRE(m.license.has_value());
    REQUIRE(m.license->empty());
}
