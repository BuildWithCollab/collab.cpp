#include <catch2/catch_test_macros.hpp>

import collab;

using namespace collab::log;

struct log_fixture {
    log_fixture() {
        clear_sinks();
        set_level(level::trace);
    }
    ~log_fixture() {
        clear_sinks();
        set_level(level::info);
    }
};

TEST_CASE("logging with no sinks does not crash", "[log]") {
    log_fixture fix;
    info("hello");
    warn("warning");
    error("error");
    critical("critical");
}
