// Header-only mode: <collab.hpp> only, no link against the static collab lib.
// Common tests only — sink-factory tests live in extended/ and require the
// linked impl.

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <collab.hpp>

#include "common/test_fixed_string.inc"
#include "common/test_identifier.inc"
#include "common/test_semver.inc"
#include "common/test_manifest.inc"
#include "common/test_publisher.inc"
#include "common/test_log.inc"
#include "common/test_error.inc"
#include "common/test_term.inc"
#include "common/test_atomic_file_writer.inc"
