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

#include "common/test_identifier.inc"
#include "common/test_semver.inc"
#include "common/test_manifest.inc"
#include "common/test_publisher.inc"
#include "common/test_log.inc"
#include "common/test_error.inc"
#include "common/test_term.inc"

#include "extended/test_log_sinks.inc"
#include "extended/test_term_ostream.inc"
