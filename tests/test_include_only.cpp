#include <catch2/catch_test_macros.hpp>

#include <collab.hpp>

using namespace collab::log;

TEST_CASE("include-only: set/get round-trip", "[include-only]") {
    set_level(level::warn);
    REQUIRE(get_level() == level::warn);
    set_level(level::trace);
    REQUIRE(get_level() == level::trace);
    set_level(level::info);
    REQUIRE(get_level() == level::info);
}
