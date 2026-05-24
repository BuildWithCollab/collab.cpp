#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//                          THE TOGGLE
//
// Uncomment the #define below to flip cppm + impl unit to the BROKEN state.
// Comment it out (current) to be in the SAFE state.
//
//   SAFE   →  tests-include ✓  tests-import ✓  tests-dual ✓
//   BROKEN →  tests-include ✓  tests-import ✓  tests-dual ✗ (C1117)
//
// SAFE state: cppm GMF-includes <collab/detail/log_decls.hpp> and re-exports
// names via `using ::collab::log::name;`. Both `#include` and `import` paths
// reach the same global-module entity. Mixing them in one TU works.
//
// BROKEN state: cppm declares `enum class level`, `struct log_state`, and the
// function signatures directly in the named module purview. They become
// distinct entities from the header's global-module-attached versions. A TU
// that does BOTH `#include <collab.hpp>` and `import collab;` collides with
// C1117 "symbol 'level' has already been defined".
//
// After flipping: rebuild from scratch (the BMI cache must be cleared):
//   Remove-Item -Recurse -Force build\.gens
//   xmake build
// ─────────────────────────────────────────────────────────────────────────────

//#define COLLAB_TOGGLE_BROKEN
