// Verifies <collab/core.hpp> works as a true header-only consumer would use it:
// this TU does NOT import the collab.core module and the test binary does NOT
// link the collab.core static library. The only link contracts are Catch2 and
// (transitively, via fmt-header-only) standard C++.

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <collab/core.hpp>

// ── Test sink that captures messages ───────────────────────────────────

class capture_sink final : public collab::log::sink {
public:
    struct entry {
        collab::log::level     lvl;
        bool                   has_id;
        collab::core::identifier id;     // populated only when has_id is true
        std::string            msg;
    };

    void write(collab::log::level lvl,
               const collab::core::identifier* id_ptr,
               std::string_view msg) override {
        entries.push_back({
            lvl,
            id_ptr != nullptr,
            id_ptr ? *id_ptr : collab::core::identifier{},
            std::string(msg),
        });
    }

    std::vector<entry> entries;
};

// ── Fixture: each test starts with a fresh sink-set and level ──────────

struct log_fixture {
    log_fixture() {
        collab::log::clear_sinks();
        collab::log::set_level(collab::log::level::trace);
    }
    ~log_fixture() {
        collab::log::clear_sinks();
        collab::log::set_level(collab::log::level::info);
    }
};

static capture_sink* install_capture() {
    auto owned = std::make_unique<capture_sink>();
    auto* raw  = owned.get();
    collab::log::add_sink(std::move(owned));
    return raw;
}

// ── Static identifier used by logger<I> tests ────────────────────────────

namespace test_lib {
    inline const collab::core::identifier identifier{
        .app_id   = "test-hpp",
        .app_name = "Header-Only Smoke Test",
        .org_id   = "purr",
        .org_name = "Purr",
        .tld      = "com",
    };

    using log = collab::log::logger<identifier>;
}

// ── Value types ────────────────────────────────────────────────────────

TEST_CASE("hpp: semver constructs and formats", "[hpp][semver]") {
    const collab::core::semver v{1, 2, 3, "rc.1"};
    REQUIRE(v.major == 1);
    REQUIRE(v.minor == 2);
    REQUIRE(v.patch == 3);
    REQUIRE(v.pre_release == "rc.1");
    REQUIRE(v.to_string() == "1.2.3-rc.1");
}

TEST_CASE("hpp: semver pre-release ordering matches SemVer §11", "[hpp][semver]") {
    using collab::core::semver;
    REQUIRE(semver{1, 0, 0, "alpha"} < semver{1, 0, 0});
    REQUIRE(semver{1, 0, 0, "alpha"} < semver{1, 0, 0, "alpha.1"});
    REQUIRE(semver{1, 0, 0, "alpha.2"} < semver{1, 0, 0, "alpha.10"});
}

TEST_CASE("hpp: identifier::bundle_id formats as tld.org_id.app_id", "[hpp][identifier]") {
    const collab::core::identifier ident{
        .app_id   = "collab-core",
        .app_name = "Collab Core",
        .org_id   = "mrowrpurr",
        .org_name = "Mrowr Purr",
        .tld      = "com",
    };
    REQUIRE(ident.bundle_id() == "com.mrowrpurr.collab-core");
}

TEST_CASE("hpp: manifest construction", "[hpp][manifest]") {
    const collab::core::manifest m{
        .identifier = {
            .app_id   = "collab-core",
            .app_name = "Collab Core",
            .org_id   = "mrowrpurr",
            .org_name = "Mrowr Purr",
            .tld      = "com",
        },
        .version     = collab::core::semver{1, 0, 0},
        .description = "Foundational C++23 library.",
        .authors     = {"Mrowr Purr"},
        .license     = "0BSD",
    };
    REQUIRE(m.identifier.bundle_id() == "com.mrowrpurr.collab-core");
    REQUIRE(m.version.to_string() == "1.0.0");
    REQUIRE(m.description.has_value());
    REQUIRE(*m.description == "Foundational C++23 library.");
    REQUIRE(m.authors.size() == 1);
    REQUIRE(*m.license == "0BSD");
}

// ── Untagged log path ──────────────────────────────────────────────────

TEST_CASE("hpp: untagged free functions dispatch with no identifier", "[hpp][log]") {
    log_fixture fix;
    auto* cap = install_capture();

    collab::log::info("plain {}", 42);

    REQUIRE(cap->entries.size() == 1);
    CHECK(cap->entries[0].lvl == collab::log::level::info);
    CHECK_FALSE(cap->entries[0].has_id);
    CHECK(cap->entries[0].msg == "plain 42");
}

TEST_CASE("hpp: level filtering suppresses below-threshold messages", "[hpp][log]") {
    log_fixture fix;
    auto* cap = install_capture();

    collab::log::set_level(collab::log::level::warn);

    collab::log::trace("t");
    collab::log::debug("d");
    collab::log::info("i");
    collab::log::warn("w");
    collab::log::error("e");
    collab::log::critical("c");

    REQUIRE(cap->entries.size() == 3);
    CHECK(cap->entries[0].msg == "w");
    CHECK(cap->entries[1].msg == "e");
    CHECK(cap->entries[2].msg == "c");
}

// ── Tagged log path (logger<I>) ────────────────────────────────────────

TEST_CASE("hpp: logger<I> dispatches with bound identifier", "[hpp][log][logger]") {
    log_fixture fix;
    auto* cap = install_capture();

    test_lib::log::info("hello {}", "world");
    test_lib::log::warn("warning {} of {}", 1, 3);

    REQUIRE(cap->entries.size() == 2);

    CHECK(cap->entries[0].lvl == collab::log::level::info);
    CHECK(cap->entries[0].has_id);
    CHECK(cap->entries[0].id.app_id == "test-hpp");
    CHECK(cap->entries[0].msg == "hello world");

    CHECK(cap->entries[1].lvl == collab::log::level::warn);
    CHECK(cap->entries[1].has_id);
    CHECK(cap->entries[1].id.bundle_id() == "com.purr.test-hpp");
    CHECK(cap->entries[1].msg == "warning 1 of 3");
}

TEST_CASE("hpp: logger<I> covers all six levels", "[hpp][log][logger]") {
    log_fixture fix;
    auto* cap = install_capture();

    test_lib::log::trace   ("t");
    test_lib::log::debug   ("d");
    test_lib::log::info    ("i");
    test_lib::log::warn    ("w");
    test_lib::log::error   ("e");
    test_lib::log::critical("c");

    REQUIRE(cap->entries.size() == 6);
    CHECK(cap->entries[0].lvl == collab::log::level::trace);
    CHECK(cap->entries[1].lvl == collab::log::level::debug);
    CHECK(cap->entries[2].lvl == collab::log::level::info);
    CHECK(cap->entries[3].lvl == collab::log::level::warn);
    CHECK(cap->entries[4].lvl == collab::log::level::error);
    CHECK(cap->entries[5].lvl == collab::log::level::critical);
    for (auto& e : cap->entries) {
        CHECK(e.has_id);
        CHECK(e.id.app_id == "test-hpp");
    }
}

TEST_CASE("hpp: multiple sinks all receive the same tagged message", "[hpp][log]") {
    log_fixture fix;
    auto* cap1 = install_capture();
    auto* cap2 = install_capture();

    test_lib::log::info("broadcast");

    REQUIRE(cap1->entries.size() == 1);
    REQUIRE(cap2->entries.size() == 1);
    CHECK(cap1->entries[0].msg == "broadcast");
    CHECK(cap2->entries[0].msg == "broadcast");
    CHECK(cap1->entries[0].id.app_id == "test-hpp");
    CHECK(cap2->entries[0].id.app_id == "test-hpp");
}
