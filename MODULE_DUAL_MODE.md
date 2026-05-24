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

  *Overload sets:* per `[namespace.udecl]`, a using-declaration is name-based and pulls the **entire overload set** for that name. One `using ::lib::name;` re-exports every overload of `name` reachable in the decls header — non-template overloads, template overloads, and any combination thereof. You do not list each overload's signature. This has been observed working on MSVC 14.50 and GCC/Clang for libraries with mixed non-template and template overloads (e.g., a `void log(std::string_view)` overload alongside a `template<typename... Args> void log(std::format_string<Args...>, Args&&...)`). MSVC has had historical bugs around this in module purviews, so if you're working on a toolchain that may not be current, an overload-set verification test is cheap: add a second overload of one exported function and call both via `import lib;`.
- **Impl unit `#include`s the inline header**, so its TU sees the inline definitions. The forced-emission construct (a const array of function-pointer-cast-to-void* values, one entry per exported function) coerces the compiler to emit those inline functions as COMDAT symbols in the compiled library archive, satisfying `import`-only consumers at link time. Header-only consumers still inline normally; COMDAT folding makes everything resolve to one address.

  The array needs two attributes to survive across compilers:
  - `[[maybe_unused]]` — suppresses the unused-variable warning on every compiler.
  - `[[gnu::used]]` (on GCC and Clang only, gated with `#if defined(__GNUC__) || defined(__clang__)`) — tells the optimizer the variable is observed externally and must not be dead-code-eliminated. Without it, `-O2` on GCC/Clang drops the entire array, which drops the address-take ODR-uses, which drops the symbol emission — and `import`-only consumers fail to link. MSVC's optimizer respects address-taken externally-linkable storage without an extra annotation; GCC/Clang's does not.

### Shared state

> ⚠️ The word "inline" appears in two completely different roles here. Read carefully.

Mutable state shared across consumer paths (e.g., a singleton registry) must be a **namespace-scope `inline` variable**:

```cpp
namespace lib::detail {
inline log_state g_state{};   // ← this kind of inline: variable, COMDAT-folded by the linker
}
```

It must **NOT** be a function-local static inside an inline function:

```cpp
namespace lib::detail {
inline log_state& state() {   // ← this kind of inline: function, dangerous for shared state
    static log_state s;        //     The `static` here is what holds the state.
    return s;
}
}
```

Why the second pattern fragments under modules: the function-local static `s` lives inside the entity `state`. When module attachment splits `state` into two entities (the global-module version reached via `#include`, the named-module version reached via `import`), each entity has its own copy of `s`. State written through one path is invisible to readers via the other. The "inline" on the function doesn't save you — inline-function-local-static deduplication is a property of the **function** being the same entity, and module attachment makes them not the same entity.

The first pattern works because the inline keyword applies to a **variable** at namespace scope. Inline variables get COMDAT semantics directly: every TU that sees the declaration emits a COMDAT definition, the linker folds them all to one address regardless of which TUs participated, and module attachment does not change the mangled name for namespace-scope variables (verified empirically on MSVC 14.50). One program, one address, one state.

**Rule of thumb:** if a shared-state pattern relies on `static` *inside* a function for its singleton-ness, it does not survive module attachment splitting. Move the storage to namespace scope and let the `inline` keyword on the variable carry the singleton guarantee.

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

## Templates

Templates work in this architecture without special treatment. The IFC consumer ICE that motivates the whole pattern is specific to **non-template inline function bodies** crossing the BMI. Template definitions are not affected — module reachability handles them correctly, and MSVC's BMI consumer parses template definitions without ICEing.

Concretely: a template lives in the inline header alongside the non-template content. The decls header carries the template **definition** (not just a forward declaration) — templates must be visible at instantiation, so the body has to remain reachable. The cppm's `using ::lib::tmpl_name;` re-exports the template the same way it re-exports a non-template name. Module consumers instantiate the template normally; the body is reachable through the using-decl chain.

This is empirically verified — real libraries built on this architecture have non-trivial template surfaces (variadic templates, NTTP-by-reference templates over user types, std/fmt-style format-string templates) and they instantiate correctly from both `#include` and `import` paths.

## `constexpr` and `consteval` functions

These need the **same treatment as templates**: bodies stay visible everywhere, never get stripped.

- **`constexpr`** functions can be evaluated at compile time *or* called at runtime. The runtime case is fine through the normal architecture (declaration in BMI, symbol emitted by the impl unit). The compile-time case requires the body to be reachable from the consumer's TU — stripping it to a forward declaration breaks any constant-expression context that calls the function.
- **`consteval`** functions must be evaluated at compile time. No runtime symbol is needed (no calls survive to the link step) — but the body must be visible at every call site.

In both cases: the decls header carries the **definition**, not just a declaration. The cppm re-exports the name via using-decl as for any other entity. The impl unit's force-emission array does NOT include `consteval` functions (no runtime symbol exists) and does not strictly need `constexpr` functions either (their bodies are reachable inline, so a consumer's runtime call inlines them rather than calling out to a `.lib` symbol — but including them does no harm).

If the canonical file has `inline constexpr R foo(args) { body }`, the generator must recognize `constexpr` (and `consteval`) as body-preserving keywords and not strip the body — same rule as `template`.

## Class member functions

The decls header carries class type definitions. Classes routinely have member functions, and how those member functions are written determines where their bodies live in this architecture.

**In-class definitions are kept verbatim in the decls header.** Methods defined inside the class body — defaulted special members, single-line accessors, simple ctors with member-init lists — are implicitly inline but tied to the class entity. MSVC's BMI carries them without triggering the IFC consumer ICE. The ICE that motivates the whole pattern is specific to **namespace-scope** inline function bodies; class-body-inline methods are not affected. Empirically, the architecture works with classes that have defaulted virtual destructors, pure-virtual member functions, simple accessors, and small constructors all defined in-class.

**Out-of-class `inline` member definitions follow the free-function pattern.** When the canonical inline header has:

```cpp
struct S {
    std::string bundle_id() const;   // declared in-class
    // ...
};

inline std::string S::bundle_id() const {   // defined out-of-class, inline
    // ... body
}
```

…the out-of-class `inline` definition is a namespace-scope inline function for the purposes of the IFC consumer ICE. Treat it identically to a free function:
- decls header: keep the in-class declaration, strip the out-of-class definition
- inline header: keep the out-of-class inline definition (this is what header consumers compile against)
- impl unit: include the inline header, force emission so the symbol lands in the `.lib`

**Practical guidance:** keep in-class definitions to trivial things (defaulted/= 0 special members, one-line accessors). Anything substantial moves out-of-class with `inline`, and gets the same treatment as a free function.

**Force-emission of member functions** uses pointer-to-member types, not regular function pointers. Pointer-to-member sizes and layouts differ from regular function pointers; do **not** `reinterpret_cast` them to `void*` (UB on most platforms and almost certainly broken on MSVC for virtual or multi-inherited classes). The simplest portable shape is a static function that ODR-uses each member address:

```cpp
namespace {
#if defined(__GNUC__) || defined(__clang__)
[[gnu::used]]
#endif
[[maybe_unused]]
inline void _emit_S_members() {
    (void)static_cast<std::string (S::*)() const>(&S::bundle_id);
    // ... one ODR-use per out-of-class inline member
}
}
```

This forces the member function's address to be observed, which forces its symbol to be emitted. The static_cast disambiguates overloaded members. The function body itself never runs at runtime.

## Macros do not cross the module boundary

`import lib;` transports declarations and entities. It does **not** transport preprocessor macros. This is by design — modules deliberately isolate consumers from the preprocessor state of the imported unit — but it means a public-API macro defined in `<lib.hpp>` is invisible to `import`-only consumers.

| Consumer | Sees `#define`d macros from `<lib.hpp>`? |
|---|---|
| `#include <lib.hpp>` only | yes |
| `import lib;` only | **no** |
| Both in same TU | yes (via the include path) |

If `<lib.hpp>` defines `LIB_VERSION_MAJOR`, `LIB_FEATURE_X_ENABLED`, or a convenience macro like `LIB_LOG_ERROR(...)`, the import-only path silently lacks them. Code that uses these macros compiles fine for `#include` consumers, fails to compile for `import` consumers, and works "by accident" for dual consumers because the `#include` path happens to come first.

**The recommended design rule: no macros in the public API.** Express the same intent with language-level constructs that *do* cross the module boundary:

| Instead of | Use |
|---|---|
| `#define LIB_VERSION_MAJOR 3` | `inline constexpr int version_major = 3;` |
| `#define LIB_FEATURE_X_ENABLED 1` | `inline constexpr bool feature_x_enabled = true;` |
| `#define LIB_ASSERT(x) ...` | a function or function template that takes `std::source_location` |
| Configuration macros (`#define LIB_USE_FOO`) | build-system options that switch the compiled implementation, not header content the consumer sees |

This is the cleanest path. New libraries should treat "the public API contains a macro" as a smell.

**For unavoidable macros**, keep them in a dedicated header that consumers are required to `#include` even when otherwise on the import path:

```cpp
// include/lib/macros.hpp
#pragma once
// Macros that cannot be expressed without the preprocessor live here.
// import lib; consumers MUST also #include <lib/macros.hpp> to use these.
#define LIB_LOG_ERROR(...) ::lib::log::error_at(__FILE__, __LINE__, __VA_ARGS__)
```

Document this prominently. The codegen doesn't help with macros — the canonical inline header should *not* define them (they'd silently disappear from the module path); the macros header is hand-maintained and consumed explicitly.

The narrow cases where macros are genuinely unavoidable:
- `__FILE__` / `__LINE__` / `__func__` capture at the call site (use `std::source_location` instead when possible — it works without a macro in C++20+).
- Stringification (`#x`) and token-pasting (`##`) of the caller's source code.
- Conditional compilation that depends on properties of the consumer's TU.

For everything else — version numbers, feature flags, type traits, constants — language-level alternatives exist and are strictly better in this architecture.

## Module-on-module composition

A library `lib` that builds on top of another modular library `other_lib` will typically have `import other_lib;` in `lib.cppm`'s purview. Two questions arise: does `using ::lib::name;` still work when `name`'s signature mentions `other_lib`-attached types, and how do consumers of `lib` see those types?

**The using-decl re-export trick holds.** A using-declaration is name-based per `[namespace.udecl]`; the type completeness of the function's signature is established at the function's declaration point (in the decls header). By the time the cppm processes `using ::lib::name;`, the function's signature — including any `other_lib`-attached types it mentions — is already fully formed. The using-decl carries the function entity through unchanged, regardless of which module the parameter or return types are attached to. This is the standard behavior; no compiler has been observed treating this case specially.

**But the consumer of `lib` needs the upstream types visible.** Re-exporting `lib::name` does not automatically re-export the upstream types its signature mentions. A consumer that calls `lib::name(some_other_lib_value)` needs `other_lib::SomeType` to be a known type in their TU. Two ways to provide that:

```cpp
// Option A: lib.cppm forwards the upstream import.
export import other_lib;

// Option B: lib.cppm re-exports specific types from upstream.
export namespace lib {
    using ::other_lib::SomeType;
}
```

Option A is the simpler "everything `lib` depends on, you also have" stance — appropriate when `other_lib` is a foundational dependency. Option B is finer-grained — only the specific types `lib` uses in its surface bubble up.

**Reachability quirks to watch for.** When you chain `export import` across several layers (`lib_a` re-exports `lib_b`, which re-exports `lib_c`), MSVC has had occasional bugs with deeply-chained reachability — types reach correctly but their members or nested types sometimes don't. If you build on a stack of modular libraries, add an import-only test in `lib_a` that constructs and uses `lib_c::DeepType` to verify the chain holds.

## Shared libraries (DLLs / .so)

The architecture is verified for **static libraries** (`.lib`, `.a`). Shipping the same library as a shared library (DLL, dynamic library, shared object) is not covered by the verification and likely needs adjustment:

- Exported symbols need `__declspec(dllexport)` (MSVC) or `__attribute__((visibility("default")))` (GCC/Clang) on their declarations to be visible across the DLL boundary. These attributes have to flow through the decls header AND the inline header consistently — if one path tags the symbol and the other doesn't, the symbol effectively splits across the boundary and consumers see "missing" exports.
- The force-emission array in the impl unit interacts with dllexport in ways that haven't been tested here. The COMDAT emission may or may not get the dllexport attribute the consumer needs; this depends on whether the address-taken function's declaration carried the attribute.
- Inline variable state (the namespace-scope `inline T g_state` pattern for shared singletons) is brittle across shared library boundaries because each DLL may end up with its own COMDAT instance rather than the linker folding to one. This is a general shared-library problem, not specific to modules — but the dual-mode architecture inherits it.

If you need DLL support, plan on additional verification work: a fourth test binary built against a DLL build of the library, plus separate diamond tests for state coherence across the DLL boundary.

## What the verification does NOT cover

- ABI stability across module/header switches. The patterns above target source-level coherence; cross-compilation-unit ABI compatibility (e.g., a library built with one module shape consumed by code built against a different module shape) is not exercised.

## Verified compilers

Architecture and toggle behavior verified on:
- MSVC 14.50 (Visual Studio 2026)
- GCC and Clang (with the `[[gnu::used]]` annotation on the force-emission array, as described above)

Differences from MSVC are noted inline where they matter (the force-emission attribute is the only one observed so far).
