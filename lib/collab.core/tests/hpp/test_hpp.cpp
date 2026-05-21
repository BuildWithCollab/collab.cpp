// Verifies <collab/core.hpp> works as a true header-only consumer would use it:
// this TU does NOT import the collab.core module and the test binary does NOT
// link the collab.core static library. The only link contracts are Catch2 and
// (transitively, via fmt-header-only) standard C++.

#include <catch2/catch_test_macros.hpp>

#include <exception>
#include <expected>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
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

// ── Error base + library/leaf hierarchy ────────────────────────────────

namespace test_lib {
    struct error : collab::core::error {
        using collab::core::error::error;
    };

    namespace errors {
        struct trivial : test_lib::error {
            using test_lib::error::error;
        };

        struct fancy : test_lib::error {
            int         code;
            std::string detail;

            fancy(int c, std::string d)
                : test_lib::error("fancy error: code {} ({})", c, d)
                , code(c)
                , detail(std::move(d))
            {}
        };
    }
}

TEST_CASE("hpp: error constructs from string_view", "[hpp][error]") {
    const collab::core::error e{std::string_view{"plain message"}};
    REQUIRE(std::string_view{e.what()} == "plain message");
}

TEST_CASE("hpp: error constructs from format string", "[hpp][error]") {
    const collab::core::error e{"connect failed: {}:{}", "example.com", 443};
    REQUIRE(std::string_view{e.what()} == "connect failed: example.com:443");
}

TEST_CASE("hpp: trivial leaf inherits format ctor via using-declaration", "[hpp][error]") {
    try {
        throw test_lib::errors::trivial{"trivial {} fired", "thing"};
    } catch (const test_lib::errors::trivial& e) {
        CHECK(std::string_view{e.what()} == "trivial thing fired");
    }
}

TEST_CASE("hpp: fancy leaf builds message from typed payload", "[hpp][error]") {
    const test_lib::errors::fancy e{42, "bad stuff"};
    CHECK(e.code == 42);
    CHECK(e.detail == "bad stuff");
    CHECK(std::string_view{e.what()} == "fancy error: code 42 (bad stuff)");
}

TEST_CASE("hpp: catch chain: leaf is reachable as leaf", "[hpp][error][catch]") {
    try {
        throw test_lib::errors::fancy{7, "x"};
    } catch (const test_lib::errors::fancy& e) {
        CHECK(e.code == 7);
        CHECK(e.detail == "x");
    } catch (...) {
        FAIL("expected fancy catch");
    }
}

TEST_CASE("hpp: catch chain: leaf catches as library base", "[hpp][error][catch]") {
    try {
        throw test_lib::errors::fancy{1, "y"};
    } catch (const test_lib::errors::trivial&) {
        FAIL("wrong leaf");
    } catch (const test_lib::error& e) {
        CHECK(std::string_view{e.what()} == "fancy error: code 1 (y)");
    }
}

TEST_CASE("hpp: catch chain: library error catches as collab::core::error", "[hpp][error][catch]") {
    try {
        throw test_lib::errors::trivial{"core-level catch"};
    } catch (const collab::core::error& e) {
        CHECK(std::string_view{e.what()} == "core-level catch");
    }
}

TEST_CASE("hpp: catch chain: collab::core::error catches as std::exception", "[hpp][error][catch]") {
    try {
        throw collab::core::error{"std-exception catch"};
    } catch (const std::exception& e) {
        CHECK(std::string_view{e.what()} == "std-exception catch");
    }
}

TEST_CASE("hpp: catch chain: collab::core::error catches as std::runtime_error", "[hpp][error][catch]") {
    try {
        throw collab::core::error{"runtime-error catch"};
    } catch (const std::runtime_error& e) {
        CHECK(std::string_view{e.what()} == "runtime-error catch");
    }
}

TEST_CASE("hpp: error usable as std::expected error type", "[hpp][error][expected]") {
    auto compute = [](bool ok) -> std::expected<int, test_lib::errors::fancy> {
        if (!ok) return std::unexpected(test_lib::errors::fancy{99, "nope"});
        return 42;
    };

    auto ok  = compute(true);
    auto bad = compute(false);

    REQUIRE(ok.has_value());
    CHECK(*ok == 42);

    REQUIRE_FALSE(bad.has_value());
    CHECK(bad.error().code == 99);
    CHECK(bad.error().detail == "nope");
    CHECK(std::string_view{bad.error().what()} == "fancy error: code 99 (nope)");
}

TEST_CASE("hpp: error usable in std::variant for expected return", "[hpp][error][expected][variant]") {
    using either = std::variant<test_lib::errors::trivial, test_lib::errors::fancy>;

    auto compute = [](int which) -> std::expected<int, either> {
        if (which == 1) return std::unexpected(either{test_lib::errors::trivial{"first"}});
        if (which == 2) return std::unexpected(either{test_lib::errors::fancy{7, "second"}});
        return 0;
    };

    auto r1 = compute(1);
    auto r2 = compute(2);
    REQUIRE_FALSE(r1.has_value());
    REQUIRE_FALSE(r2.has_value());

    REQUIRE(std::holds_alternative<test_lib::errors::trivial>(r1.error()));
    CHECK(std::string_view{std::get<test_lib::errors::trivial>(r1.error()).what()} == "first");

    REQUIRE(std::holds_alternative<test_lib::errors::fancy>(r2.error()));
    const auto& f = std::get<test_lib::errors::fancy>(r2.error());
    CHECK(f.code == 7);
    CHECK(f.detail == "second");
}
