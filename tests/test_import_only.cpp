#include <catch2/catch_test_macros.hpp>

import collab;

using namespace collab::log;

TEST_CASE("import-only: set/get round-trip", "[import-only]") {
    set_level(level::warn);
    REQUIRE(get_level() == level::warn);
    set_level(level::trace);
    REQUIRE(get_level() == level::trace);
    set_level(level::info);
    REQUIRE(get_level() == level::info);
}
