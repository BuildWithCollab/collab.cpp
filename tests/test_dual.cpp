#include <catch2/catch_test_macros.hpp>

#include <collab.hpp>
import collab;

TEST_CASE("dual: #include + import — does state stay coherent across paths?", "[dual]") {
    using namespace collab::log;

    set_level(level::warn);
    REQUIRE(get_level() == level::warn);

    set_level(level::info);
    REQUIRE(get_level() == level::info);

    set_level(level::trace);
    REQUIRE(get_level() == level::trace);
}
