// Module mode: `import collab;` only. Links the static collab library.
// Full API surface — common + extended tests.

#include <catch2/catch_test_macros.hpp>

// Std headers used by tests must be included BEFORE `import collab;` —
// re-including a std header after the module's GMF already processed it
// triggers MSVC C7571/C2766 (duplicate variable-template specializations).
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
