# Dual-Mode Library Codegen: Minimal Template Design

The dual-mode architecture (see `MODULE_DUAL_MODE.md`) requires four coordinated files per area of a library. Maintained by hand, each new function touches all four files. This document specifies the smallest mechanical transformation that removes the duplication.

The design assumes you've read `MODULE_DUAL_MODE.md` and understand *why* the four files exist. This document is about *how to keep them in sync without writing them all*.

---

## Example: library `widget` with two areas

Concrete picture before any prose. Library named `widget`, two areas: `color` and `shape`. Legend: ✏️ = author writes, 🤖 = generator emits.

```
widget/
├── include/
│   └── widget/
│       ├── widget.hpp                  ✏️  umbrella, #includes each area header
│       ├── color.hpp                   ✏️  canonical for color area
│       ├── shape.hpp                   ✏️  canonical for shape area
│       └── detail/
│           ├── color.decls.hpp         🤖  color, bodies stripped
│           └── shape.decls.hpp         🤖  shape, bodies stripped
├── src/
│   ├── widget.cppm                     ✏️  primary, aggregates partitions
│   ├── color.cppm                      🤖  color partition (using-decls)
│   ├── color_impl.cpp                  🤖  color impl unit (address-take)
│   ├── shape.cppm                      🤖  shape partition
│   └── shape_impl.cpp                  🤖  shape impl unit
├── scripts/
│   └── generate.py                     ✏️  the generator script itself
└── tests/
    ├── test_include_only.cpp           ✏️  binary 1: #include widget/widget.hpp only
    ├── test_import_only.cpp            ✏️  binary 2: import widget; only
    └── test_dual.cpp                   ✏️  binary 3: both, in same TU
```

What the author touches when adding a function: edit `include/widget/color.hpp` (one line), re-run `scripts/generate.py`. The five 🤖 files update mechanically.

### What's inside each file

**✏️ `include/widget/color.hpp`** — author writes this:
```cpp
#pragma once
#include <string_view>
namespace widget::color {
enum class rgb { red, green, blue };
inline rgb current = rgb::red;
inline void set(rgb c)        { current = c; }
inline rgb  get()              { return current; }
inline void from_name(std::string_view) { /* ... */ }
}
```

**🤖 `include/widget/detail/color.decls.hpp`** — generator emits:
```cpp
#pragma once
#include <string_view>
namespace widget::color {
enum class rgb { red, green, blue };
extern rgb current;
void set(rgb);
rgb  get();
void from_name(std::string_view);
}
```

**🤖 `src/color.cppm`** — generator emits:
```cpp
module;
#include <widget/detail/color.decls.hpp>
export module widget:color;
export namespace widget::color {
using ::widget::color::rgb;
using ::widget::color::current;
using ::widget::color::set;
using ::widget::color::get;
using ::widget::color::from_name;
}
```

**🤖 `src/color_impl.cpp`** — generator emits:
```cpp
module;
#include <widget/color.hpp>
module widget;
namespace {
#if defined(__GNUC__) || defined(__clang__)
[[gnu::used]]
#endif
[[maybe_unused]] const void* const _emit_color_symbols[] = {
    reinterpret_cast<const void*>(static_cast<void(*)(widget::color::rgb)>(&widget::color::set)),
    reinterpret_cast<const void*>(static_cast<widget::color::rgb(*)()>(&widget::color::get)),
    reinterpret_cast<const void*>(static_cast<void(*)(std::string_view)>(&widget::color::from_name)),
};
}
```

**✏️ `src/widget.cppm`** — author writes this (two lines per area):
```cpp
export module widget;
export import :color;
export import :shape;
```

**✏️ `include/widget/widget.hpp`** — author writes this (one line per area):
```cpp
#pragma once
#include <widget/color.hpp>
#include <widget/shape.hpp>
```

`shape.hpp` and its generated counterparts follow the identical pattern.

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

The canonical inline header with bodies stripped — but only for plain `inline` functions. Any function whose declaration carries `constexpr`, `consteval`, or `template` keeps its body, because the body must remain visible at instantiation / constant evaluation.

| Source line | Transformed to |
|---|---|
| `inline R foo(args...) { body }` | `R foo(args...);` |
| `inline T name = value;` | `extern T name;` |
| `inline constexpr R foo(args...) { body }` | **unchanged** (constexpr body must stay reachable) |
| `inline consteval R foo(args...) { body }` | **unchanged** (consteval body must stay reachable) |
| `template <...> R foo(args...) { body }` | **unchanged** (templates remain inline-visible) |
| `template <...> struct/class S { ... };` | **unchanged** |
| `enum class E { ... };` | unchanged |
| `struct/class S { ... };` | unchanged |
| namespace open/close, comments, includes | unchanged |

Result: a header containing every type definition the canonical file declares, every function signature, every variable declaration, plus the **full bodies of every template, `constexpr`, and `consteval` function** — and no bodies that could end up in the module's BMI for plain inline functions. See `MODULE_DUAL_MODE.md` for why templates and constexpr/consteval need body preservation.

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
#if defined(__GNUC__) || defined(__clang__)
[[gnu::used]]
#endif
[[maybe_unused]] const void* const _emit_<area>_symbols[] = {
    reinterpret_cast<const void*>(static_cast<sig1>(&::lib::<area>::name1)),
    reinterpret_cast<const void*>(static_cast<sig2>(&::lib::<area>::name2)),
    // ... one entry per non-template function
};
}
```

The address-take array forces the compiler to emit the inline function bodies as out-of-line COMDAT symbols in the library's compiled archive, satisfying `import`-only consumers at link time. The `[[gnu::used]]` attribute (GCC/Clang only) prevents `-O2` from dead-code-eliminating the array; MSVC respects the address-take without it. See `MODULE_DUAL_MODE.md` for details.

---

## What's NOT generated

- **The primary aggregator `src/lib.cppm`**: hand-written, ~one line per area:
  ```cpp
  export module lib;
  export import :area1;
  export import :area2;
  ```
- **The umbrella `include/lib.hpp`**: hand-written, `#include`s the per-area headers.
- **Templates, `constexpr`, `consteval`**: copied verbatim into the decls header — the generator does no transformation on these. They work end-to-end through the architecture without special treatment; the IFC consumer ICE is specific to plain non-template inline function bodies, not these. The decls header carries the full body (must be visible at instantiation / constant evaluation), the cppm's using-decl re-exports the name, and consumers use them normally. See the templates and constexpr/consteval sections of `MODULE_DUAL_MODE.md`.
- **DLL export attributes** (`__declspec(dllexport)`, visibility attributes). The generator targets static-library output. Shared-library builds need attribute decoration on declarations in both the inline header and the decls header consistently; the generator does not currently emit these, and the force-emission array's interaction with dllexport is unverified. See the shared-libraries section of `MODULE_DUAL_MODE.md`.
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
