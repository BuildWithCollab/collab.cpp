// Dual mode: BOTH `#include <collab.hpp>` and `import collab;` in the same TU.
// The architecture's load-bearing test — fails (C1117 on MSVC, ODR elsewhere)
// if the module's exported entities are not the same entities the header
// declares.
//
// Links the static collab library. Full API surface — common + extended tests.

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>

#include <collab.hpp>
import collab;

#include "common/test_fixed_string.inc"
#include "common/test_identifier.inc"
#include "common/test_semver.inc"
#include "common/test_manifest.inc"
#include "common/test_publisher.inc"
#include "common/test_log.inc"
#include "common/test_error.inc"
#include "common/test_term.inc"

#include "extended/test_log_sinks.inc"
#include "extended/test_term_ostream.inc"

// Direct std::format on a fixed_string — exercises the std::formatter partial
// spec from the canonical header. Lives inline in this TU because it doesn't
// fit `common/` (MSVC modules don't surface the spec to pure-import TUs) and
// doesn't fit `extended/` (no linked impl required).

#include <format>

TEST_CASE("fixed_string: std::format directly on fixed_string", "[fixed_string]") {
    collab::fixed_string s = "world";
    auto out = std::format("hello {}", s);
    REQUIRE(out == "hello world");
}

TEST_CASE("fixed_string: fmt::format directly on fixed_string", "[fixed_string]") {
    collab::fixed_string s = "world";
    auto out = fmt::format("hello {}", s);
    REQUIRE(out == "hello world");
}

