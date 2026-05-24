# C++23 Module Dual-Mode Libraries: Architecture and Verification

A library that ships **both** a header (for `#include`) and a C++23 module (for `import`) must work in three consumer modes:

1. `#include <lib.hpp>` only
2. `import lib;` only
3. **Both, in the same translation unit**

Mode 3 is the hard one. Getting modes 1 and 2 to pass independently is easy and tells you nothing about mode 3 — they routinely pass even when the dual-mode architecture is wrong. This document is the objective data on what fails, how to avoid it, and how to *prove* your library actually supports mode 3.

---

## Failure modes (empirically observed on MSVC 14.50 / VS 2026)

### IFC Consumer ICE (C1001)

```
fatal error C1001: Internal compiler error.
note: IFC import detected.
```

**Trigger.** A TU does `import lib;` against a module whose BMI carries inline function bodies. Most commonly the BMI was populated via `export import <lib.hpp>;` (header-unit re-export), but any mechanism that lands inline definitions in the module's exported interface produces the same ICE.

**Where it surfaces.** Often at the closing `};` of a struct or class whose ctor/dtor calls inline functions reached through the imported module. The error line points at the brace, not the call site — MSVC's IFC consumer is choking while reconstructing the imported entities at the point of class completion.

**Architectural meaning.** Inline function bodies must not cross the module export boundary. The BMI must carry declarations only.

### Symbol Already Defined (C1117)

```
fatal error C1117: unrecoverable error importing module 'lib':
symbol 'X' has already been defined.
note: IFC import detected.
```

**Trigger.** A TU does BOTH `#include <lib.hpp>` AND `import lib;`, and the module's exported type `X` is a *different entity* than the header's declared type `X`. Per `[basic.link]/9.3`, declarations attached to different modules are distinct entities. The header's `X` is global-module-attached; if the module re-declares `X` in its own purview, the module's `X` is named-module-attached. MSVC sees two declarations of the same qualified name, refuses to merge them, and surfaces C1117 at the first call site that uses `X`.

**Architectural meaning.** The module's exported entities must refer to the *same* entities the header declares — not freshly redeclared in the named module.

### Unresolved External (LNK2019)

```
LNK2019: unresolved external symbol "void __cdecl lib::foo(...)"
referenced in function ...
```

**Trigger.** An `import`-only consumer fails to link. The module BMI declared `foo` (no body); the consumer's call site expects a real symbol at link time; nothing in the library's compiled `.lib` emits `foo`. Common cause: the impl unit `#include`s a header full of inline `foo` definitions, but since the impl unit never *uses* `foo`, the optimizer drops the inline bodies and no symbol lands in the `.lib`.

**Architectural meaning.** When the BMI carries declarations only, the library must guarantee the corresponding symbols exist in its compiled output.

---

## Architecture pattern

Four files per area of the library:

| Role | Path (example) | Contents |
|---|---|---|
| Decls header | `include/lib/detail/decls.hpp` | Type definitions (enums, structs, classes) + function signatures. **No bodies, no `inline`.** |
| Inline header | `include/lib.hpp` (or `include/lib/area.hpp`) | `#include`s the decls header. Adds `inline` namespace-scope variables for shared state. Adds inline function bodies. |
| Module interface unit | `src/lib.cppm` | `module;` + `#include <lib/detail/decls.hpp>` + `export module lib;` + `export namespace lib { using ::lib::name; ... }` per exported name. |
| Module impl unit | `src/lib_impl.cpp` | `module;` + `#include <lib.hpp>` + `module lib;` + a forced-emission construct (see below). |

### Why each piece exists

- **Decls header is the single source of type declarations.** Both the inline header and the cppm GMF include it. A TU that does `#include + import` sees the same declaration of every type from both paths — same entity, no C1117.
- **Inline header is what `#include` consumers get.** Inline bodies in their TUs as normal. Inline namespace-scope variables for state get COMDAT-deduplicated across TUs by the linker.
- **cppm declares no entities of its own.** It re-exports via `using ::lib::name;`. The using-decl carries a reference to the global-module entity declared in the decls header, not a fresh named-module declaration. BMI carries no inline bodies → no IFC ICE.
- **Impl unit `#include`s the inline header**, so its TU sees the inline definitions. The forced-emission construct (a const array of function-pointer-cast-to-void* values, one entry per exported function) coerces the compiler to emit those inline functions as COMDAT symbols in the compiled library archive, satisfying `import`-only consumers at link time. Header-only consumers still inline normally; COMDAT folding makes everything resolve to one address.

  The array needs two attributes to survive across compilers:
  - `[[maybe_unused]]` — suppresses the unused-variable warning on every compiler.
  - `[[gnu::used]]` (on GCC and Clang only, gated with `#if defined(__GNUC__) || defined(__clang__)`) — tells the optimizer the variable is observed externally and must not be dead-code-eliminated. Without it, `-O2` on GCC/Clang drops the entire array, which drops the address-take ODR-uses, which drops the symbol emission — and `import`-only consumers fail to link. MSVC's optimizer respects address-taken externally-linkable storage without an extra annotation; GCC/Clang's does not.

### Shared state

Mutable state shared across consumer paths (e.g., a singleton registry) must be a **namespace-scope `inline` variable**, not a function-local static inside an inline function. Function-local statics live inside a function entity; when module attachment splits the function into two entities (global-module via header, named-module via import), each entity has its own static and state fragments. Namespace-scope `inline` variables get COMDAT-deduplicated by the linker regardless of which TUs see them — one address per program.

---

## Verification methodology

A library claiming to support dual-mode MUST ship three test binaries and a toggle.

### The three binaries

| Binary | Source TU | Tests |
|---|---|---|
| `tests-include` | `#include <lib.hpp>` only | API works via header path |
| `tests-import` | `import lib;` only | API works via module path |
| `tests-dual` | BOTH in the same TU | API works when paths are mixed |

All three binaries exercise the same minimal API surface. All three must build and pass.

### The toggle

Three passing binaries do not prove the architecture is doing real work — they might all pass by accident, with a degenerate setup that happens to be coherent. To prove the dual binary's success is load-bearing, the project must provide a way to deliberately break the dual-mode-supporting architecture while leaving the include and import binaries intact:

- A `#define LIB_TOGGLE_BROKEN` (or an xmake/CMake build option) that switches the cppm + impl unit between:
  - **SAFE**: cppm GMF-includes the shared decls header and `using ::lib::name;` re-exports. Impl unit `#include`s the inline header.
  - **BROKEN**: cppm declares types directly in its named-module purview (no decls header, no using-decls). Impl unit self-contains its definitions using those named-module types.

Expected behavior:

| | TOGGLE = SAFE | TOGGLE = BROKEN |
|---|---|---|
| tests-include | pass | pass |
| tests-import | pass | pass |
| tests-dual | pass | **fail (C1117)** |

If the toggle does not produce this matrix, one of three things is true:

1. The architecture is more redundant than needed — both states accidentally work and you don't know why.
2. The dual test is not exercising the dual-mode failure surface.
3. There's a subtle bug masking the failure.

Diagnose before committing.

### Workflow

For any new library or significant modification:

1. Build the four-file architecture (decls header, inline header, cppm, impl unit).
2. Write three test TUs exercising the same minimal API.
3. Build all three binaries; confirm pass.
4. Add the toggle. Flip to BROKEN. Confirm: include + import pass, dual fails with the expected error.
5. Flip back to SAFE. Confirm 3/3 pass.
6. Commit with the toggle in SAFE state.

Skipping step 4 is the silent failure mode. A library committed without the toggle proven is a library whose dual-mode support is unverified.

### Cross-TU state coherence (optional, but recommended for libraries with shared state)

A four-TU diamond test:

- TU A: `import lib;` — modifies shared state
- TU B: `#include <lib.hpp>` — reads shared state
- Test asserts the value written by A is observed by B (and vice versa)

This is what verifies the `inline` namespace-scope variable is actually being COMDAT-folded across attachment boundaries. Single-TU dual tests typically resolve both reads and writes to the same entity (which is the *point* of the architecture), so they don't distinguish whether state coherence is real or accidental. The diamond test does.

---

## Common pitfalls

- **`#include <header.h>` after `module ...;` in the purview.** Causes massive STL redefinition errors (C2953, C2011, etc.). Standard headers must go in the GMF, before the module declaration.

- **Header units (`import <header.hpp>;`) on MSVC.** Documented support exists but is fragile in mixed `#include`/`import` scenarios. Several open ICEs as of MSVC 14.50. Prefer the decls-header + using-decl pattern.

- **`__declspec(selectany)` on functions or types.** Rejected with C2496 ("can only be applied to data items with external linkage") and C4091 (ignored on type declarations). Only valid on namespace-scope variables.

- **BMI cache survives across architecture changes.** When toggling the cppm shape, MSVC's cached `.ifc` may be incompatible with the new build. Symptom: `error C2235: mismatching target architecture for compiled module interface`. Fix: clear the BMI cache (e.g., delete the build's generated-files directory) before rebuilding after any structural change to the cppm or its dependencies.

- **Forgetting to force symbol emission in the impl unit.** Inline bodies in the impl unit's TU get optimized away — no symbol in the compiled library → `import`-only consumers fail at link with `LNK2019` (MSVC) or `undefined reference` (GCC/Clang). Take addresses of every exported function in a const array.

- **GCC/Clang dead-code-eliminating the force-emission array.** `[[maybe_unused]]` alone is not enough on GCC/Clang at `-O2`+ — the optimizer happily strips a variable that is never read, even if its initializer takes addresses. Add `[[gnu::used]]` (guarded by `#if defined(__GNUC__) || defined(__clang__)`) to force the variable's emission, which in turn forces the address-take ODR-uses to materialize as real symbols. MSVC does not need this annotation.

---

## What the verification does NOT cover

- Templates exported through the BMI. Templates must be visible at instantiation; this is a separate problem from non-template inline functions and may need additional architectural treatment.
- ABI stability across module/header switches. The patterns above target source-level coherence; if the library is consumed across compilation boundaries (DLLs, separately compiled static libs), additional ABI considerations apply.

## Verified compilers

Architecture and toggle behavior verified on:
- MSVC 14.50 (Visual Studio 2026)
- GCC and Clang (with the `[[gnu::used]]` annotation on the force-emission array, as described above)

Differences from MSVC are noted inline where they matter (the force-emission attribute is the only one observed so far).
