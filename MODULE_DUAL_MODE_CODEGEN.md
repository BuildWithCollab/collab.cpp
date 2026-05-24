# Dual-Mode Library Codegen: Minimal Template Design

The dual-mode architecture (see `MODULE_DUAL_MODE.md`) requires four coordinated files per area of a library. Maintained by hand, each new function touches all four files. This document specifies the smallest mechanical transformation that removes the duplication.

The design assumes you've read `MODULE_DUAL_MODE.md` and understand *why* the four files exist. This document is about *how to keep them in sync without writing them all*.

---

## What the author writes

For each area of the library, **one file**:

```
include/lib/<area>.hpp
```

This is regular, includable C++23. It is, in fact, the inline header that header-only consumers `#include`. It contains:

- Type definitions (enums, structs, classes)
- `inline` namespace-scope variables (shared state)
- `inline` non-template function definitions (the API surface bodies)
- Templates (if any)

No special syntax, no DSL, no comment markers. A reader who knows nothing about modules sees a normal header.

---

## What the generator produces

From each `include/lib/<area>.hpp`, three generated files:

```
include/lib/detail/<area>.decls.hpp    ← declarations only
src/<area>.cppm                         ← module interface partition
src/<area>_impl.cpp                     ← module impl unit
```

### 1. `include/lib/detail/<area>.decls.hpp`

The canonical inline header with bodies stripped:

| Source line | Transformed to |
|---|---|
| `inline R foo(args...) { body }` | `R foo(args...);` |
| `inline T name = value;` | `extern T name;` |
| `enum class E { ... };` | unchanged |
| `struct/class S { ... };` | unchanged |
| `template <...>` (any) | unchanged (templates remain inline-visible) |
| namespace open/close, comments, includes | unchanged |

Result: a header containing every type definition the canonical file declares, every function signature, every variable declaration — and no bodies that could end up in the module's BMI.

### 2. `src/<area>.cppm`

```cpp
module;
#include <lib/detail/<area>.decls.hpp>
export module lib:<area>;

export namespace lib::<area> {
    using ::lib::<area>::name1;
    using ::lib::<area>::name2;
    // ... one using-decl per top-level entity declared in the canon
}
```

The using-decls re-export the global-module entities declared in the decls header. Module consumers reach the same entities the header path declares.

### 3. `src/<area>_impl.cpp`

```cpp
module;
#include <lib/<area>.hpp>
module lib;

namespace {
[[maybe_unused]] const void* const _emit_<area>_symbols[] = {
    reinterpret_cast<const void*>(static_cast<sig1>(&::lib::<area>::name1)),
    reinterpret_cast<const void*>(static_cast<sig2>(&::lib::<area>::name2)),
    // ... one entry per non-template function
};
}
```

The address-take array forces MSVC to emit the inline function bodies as out-of-line COMDAT symbols in the library's compiled `.lib`, satisfying `import`-only consumers at link time.

---

## What's NOT generated

- **The primary aggregator `src/lib.cppm`**: hand-written, ~one line per area:
  ```cpp
  export module lib;
  export import :area1;
  export import :area2;
  ```
- **The umbrella `include/lib.hpp`**: hand-written, `#include`s the per-area headers.
- **Templates**: copied verbatim to the decls header (templates must remain inline-visible). The generator does no transformation on template definitions. If a complex template triggers an IFC ICE, the project must handle it out-of-band (e.g., keep the template header-only-consumer-accessible only).
- **The verification toggle**: project-level, separate from codegen. See `MODULE_DUAL_MODE.md`.

---

## Convention for canonical files

To keep the generator small and regex/state-machine-based (rather than a full C++ parser), canonical files follow a strict convention:

1. **One declaration per line.** No chained declarations like `int a, b;`. No multiple functions on one line.
2. **`inline` is the first keyword** on lines that declare inline functions or inline variables. (Not `static inline`, not `[[nodiscard]] inline`. If attributes are needed, place them after `inline`.)
3. **Namespaces opened with explicit braces**, one open/close per line:
   ```cpp
   namespace lib::area {
   ...
   }
   ```
   No inline namespace tricks, no `namespace ::lib { namespace area { ... } }` style.
4. **No `using namespace`** at file scope. (Using-declarations like `using std::vector;` are fine.)
5. **Function bodies brace-balanced** and parseable by a simple depth counter — no preprocessor-conditional braces inside function bodies, no string literals containing unmatched braces that the scanner would have to handle specially.

The convention is documented in a header comment in each canonical file. A canonical file that violates the convention produces a clear error from the generator, not silently-wrong output.

---

## Generator implementation

A single script (~100–200 lines, language of choice — Python, PowerShell, Lua, whatever the project's build system already uses). Algorithm:

1. **Scan line by line**, tracking namespace nesting depth and brace depth.
2. **At namespace scope** (brace depth equals namespace depth), classify each line:
   - Starts with `inline` and contains `(` before `=` → inline function definition. Find matching closing `}`, emit `signature;` to decls, record name + signature for cppm using-decl and impl unit address-take.
   - Starts with `inline` and contains `=` before `(` → inline variable definition. Emit `extern type name;` to decls, record name for cppm using-decl.
   - Starts with `enum`, `struct`, `class`, `template` → type/template definition. Pass through unchanged to decls.
   - Anything else (comments, blank lines, `using` decls, etc.) → pass through unchanged.
3. **At deeper scope** (inside types or function bodies), pass through unchanged.
4. **Emit** the three target files at end of scan.
5. **Exit code** 0 on success, non-zero with a line-numbered diagnostic on convention violation.

The script is small enough to read end-to-end in one sitting. The state machine is small enough to debug with print statements.

---

## Where the generator runs

Two integration models:

- **Pre-build hook**: build system invokes the generator before any compilation. Generated files always reflect the current canonical. Downstream consumers building from source need the generator available.
- **Author-invoked, output committed**: the author runs the generator after each change to a canonical file. Generated outputs are committed to the repository. Downstream consumers and CI build without ever running the generator.

The second model is preferred for libraries shipped as packages — consumers get a self-contained source tree with no toolchain prerequisite beyond a C++23 compiler.

---

## Why this design

**Single source of truth.** Every function body, type definition, and inline variable lives in exactly one place — the canonical inline header. Adding a function means adding one line to the canon and re-running the generator. The four generated artifacts are mechanical.

**No DSL, no markers, no annotations.** The canonical file is regular C++23. A reader doesn't need to learn a new format. IDEs index it normally. Existing tooling (formatters, linters, IDE inlay hints) works without adaptation.

**Replaceable generator.** The convention is conservative C++ that any future tool can parse. A libclang-based replacement can drop into the same pipeline without changing the canonical format or the generated file layout. The architecture survives the tooling.

**The architecture works without the generator.** If the generator breaks or the project temporarily needs hand-edits, the four-file pattern is still valid C++23. The generator is an efficiency tool, not a load-bearing dependency. Worst case, the maintainer edits all four files until the generator is restored.

**Forward-compatible with compiler evolution.** When MSVC eventually fixes the IFC consumer ICE for inline-bodies-in-BMI, the architecture may be simplifiable. The canonical file format doesn't change — only the generated outputs do. A new generator version produces a different (simpler) architecture from the same canonical source.

---

## What to ship in a new library project

1. `include/lib/<area>.hpp` files (canonical, hand-written)
2. `include/lib.hpp` (umbrella, hand-written)
3. `src/lib.cppm` (primary, hand-written, one `export import` per area)
4. `scripts/generate.<ext>` (the generator, written once per project)
5. The three test binaries from `MODULE_DUAL_MODE.md` (include-only, import-only, dual)
6. The verification toggle (per `MODULE_DUAL_MODE.md`)
7. CI step: run the generator and verify the generated files match what's committed (catches "author forgot to regenerate")

Everything else — decls headers, partition cppms, impl unit cpps — is generated.
