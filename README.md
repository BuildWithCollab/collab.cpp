# collab-core 🏴‍☠️

C++23 library for the **Collab** stack.

---

## Table of contents

- [Getting started](#getting-started)
- [Library conventions](#library-conventions)
- [Identifier and manifest](#identifier-and-manifest)
- [Semantic versioning](#semantic-versioning)
- [Logging](#logging)
- [Errors](#errors)
- [Publishers](#publishers)
- [Atomic file writer](#atomic-file-writer)
- [Terminal styling](#terminal-styling)
- [Fixed string](#fixed-string)
- [License](#license)

---

## Getting started

```cpp
#include <collab.hpp>
import collab;
```

Everything lives under `collab::` (types, error, publisher), `collab::log::` (logging API), and `collab::term::` (terminal styling).

---

## `#include <collab.hpp>` vs `import collab;`

| Feature                                                  | `#include <collab.hpp>` | `import collab;` |
| -------------------------------------------------------- | :---------------------: | :--------------: |
| [Identifier and manifest](#identifier-and-manifest)      | ✅                      | ✅               |
| [Semantic versioning](#semantic-versioning)              | ✅                      | ✅               |
| [Errors](#errors)                                        | ✅                      | ✅               |
| [Publishers](#publishers)                                | ✅                      | ✅               |
| [Atomic file writer](#atomic-file-writer)                | ✅                      | ✅               |
| [Logging](#logging)                                      | ⚠️                      | ✅               |
| [Terminal styling](#terminal-styling)                    | ⚠️                      | ✅               |
| [Fixed string](#fixed-string)                            | ✅                      | ✅               |

✅ available · ⚠️ partial · ❌ not available

### Compilers requiring `#include` alongside `import`

On some older toolchains `import collab;` alone is not sufficient — the module's GMF doesn't surface certain declarations to importer translation units. On these compilers, add `#include <collab.hpp>` alongside the import (order doesn't matter):

```cpp
#include <collab.hpp>
import collab;
```

Affected compilers and what fails without the include:

- **MSVC VS2022** — the `std::hash` / `std::formatter` / `fmt::formatter` specializations for `fixed_string` aren't visible. Calling `std::format("{}", fs)` or `std::hash<collab::fixed_string<N>>{}(fs)` fails to compile.
- **GCC 14** — namespace-scope deduction guides aren't visible. Writing `collab::fixed_string s = "hello"` or any other CTAD on `basic_fixed_string` fails to compile.

VS2026, Clang, and GCC 15+ have neither limitation — `import collab;` alone is enough on those toolchains.

---

## Library conventions

A library built on `collab-core` is expected to expose five names from its top-level namespace: **`manifest`**, **`identifier`**, **`version`**, **`log`**, and **`error`**. Declare them once at the library's root and reuse them everywhere:

```cpp
namespace mylib {
    inline const collab::manifest manifest{
        .identifier = {
            .app_id   = "mylib",
            .app_name = "My Lib",
            .org_id   = "mrowrpurr",
            .org_name = "Mrowr Purr",
            .tld      = "com",
        },
        .version     = {0, 1, 0},
        .description = "Does the thing.",
        .authors     = {"Mrowr Purr"},
        .license     = "0BSD",
    };

    inline const auto& identifier = manifest.identifier;
    inline const auto& version    = manifest.version;

    using log = collab::log::logger<identifier>;

    struct error : collab::error {
        using collab::error::error;
    };
}
```

Each of those five names earns its place by what it lets the rest of the library do:

- `identifier` and `version` are views that never drift from the manifest — one declaration, two views, no duplication.
- `log` is bound to this library's identifier at compile time. Every log call is automatically attributed; library code never names the identifier again.
- `error` anchors the library's exception hierarchy. Specific error types live in a nested `errors` namespace and inherit from it, so callers can catch at any level of specificity.

### Logging from library code

With `log` declared, anywhere in the library can just call it:

```cpp
namespace mylib {
    void connect(std::string_view host, int port) {
        log::info("connecting to {}:{}", host, port);
        log::warn("retry attempt {}", n);
    }
}
```

Every entry carries the library's identifier without the call site repeating it. The application installs sinks at startup and decides where output ends up; libraries never deal with sinks. With no sinks installed, calls are silently no-ops — library code can log freely whether or not anyone is listening.

### Throwing errors from library code

Specific error types live in `mylib::errors::` and inherit from the library's `error` base. Two patterns cover everything:

```cpp
namespace mylib::errors {
    // No extra fields — inherit the base ctors via using-declaration.
    // Throw with a raw string or a fmt::format_string.
    struct timeout : mylib::error {
        using mylib::error::error;
    };

    // Typed payload — write the ctor, build the message from the fields.
    // Caught objects expose the payload as members.
    struct connect_failed : mylib::error {
        std::string host;
        int         port;

        connect_failed(std::string h, int p)
            : mylib::error("failed to connect to {}:{}", h, p)
            , host(std::move(h))
            , port(p)
        {}
    };
}
```

At the throw site:

```cpp
namespace mylib {
    void connect(std::string_view host, int port) {
        log::info("connecting to {}:{}", host, port);
        if (!reachable(host, port))
            throw mylib::errors::connect_failed{std::string(host), port};
        if (timed_out())
            throw mylib::errors::timeout{"connect to {} timed out after {}s", host, 30};
    }
}
```

Callers catch at whatever level fits — the leaf type when they need the payload, `mylib::error` for any error from this library, `collab::error` for any error from any library built on `collab-core`. See [Errors](#errors) for the full catch chain and `std::expected` usage.

### Log levels

| Level      | Use for                                                              | Example                                       |
| ---------- | -------------------------------------------------------------------- | --------------------------------------------- |
| `trace`    | Step-by-step internals you'd only want when debugging in detail.     | `log::trace("entering parse_header()");`      |
| `debug`    | Verbose detail useful while diagnosing a specific problem.           | `log::debug("payload bytes: {}", hex);`       |
| `info`     | Normal-operation milestones a healthy system emits.                  | `log::info("listening on :{}", port);`        |
| `warn`     | Something unexpected — but the system is recovering or continuing.   | `log::warn("retry {}/{}", n, max);`           |
| `error`    | A specific operation failed; the system as a whole may still be ok.  | `log::error("connect failed: {}", err);`      |
| `critical` | System integrity at risk; immediate human attention.                 | `log::critical("disk full — aborting");`      |

---

## Identifier and manifest

`collab::identifier` carries the bits a library uses to derive paths, bundle IDs, and folder names — app slug + display name, org slug + display name, and the reverse-DNS root segment. `manifest` composes an identifier with descriptive metadata: version, description, authors, license.

A library that hasn't grown descriptive metadata yet can declare just an identifier:

```cpp
collab::identifier ident{
    .app_id   = "mylib",
    .app_name = "My Lib",
    .org_id   = "mrowrpurr",
    .org_name = "Mrowr Purr",
    .tld      = "com",
};
assert(ident.bundle_id() == "com.mrowrpurr.mylib");
```

`identifier::bundle_id()` produces the reverse-DNS form `tld.org_id.app_id`.

A `manifest` adds version, description, authors, and license on top — see the example in [Library conventions](#library-conventions). `description` and `license` are `std::optional<std::string>`; absence is distinct from an explicitly empty value. `authors` is a plain `std::vector<std::string>` because an empty vector already means "zero authors" with no ambiguity.

---

## Semantic versioning

`collab::semver` is an aggregate holding `major`, `minor`, `patch`, plus optional `pre_release` and `build` strings. Three-way comparison follows [SemVer 2.0](https://semver.org); the `build` field is ignored (per §10). `to_string()` produces the canonical `MAJOR.MINOR.PATCH[-PRE][+BUILD]` form.

```cpp
collab::semver v{1, 2, 0, "rc.1"};
assert(v < collab::semver{1, 2, 0});         // rc.1 precedes the release
assert(v.to_string() == "1.2.0-rc.1");
```

---

## Logging

> ⚠️ **Header-only consumers** get the full API and subclass `collab::log::sink` to provide output.
>
> The built-in `make_*_sink` factories below are the only piece that needs the linked impl.

Library code never deals with sinks — it just calls `log::info(...)` (see [Library conventions](#library-conventions)). The **application** at the top of the stack is responsible for installing sinks, choosing the level, and deciding where output ends up.

```cpp
int main() {
    collab::log::add_sink(collab::log::make_stdout_color_sink());
    collab::log::add_sink(collab::log::make_file_sink("app.log"));
    collab::log::set_level(collab::log::level::debug);
    // ...
}
```

Console sinks prefix with the caller's display name (`[Collab Net]`), file sinks prefix with the bundle ID (`[com.mrowrpurr.collab-net]`, for grep).

With no sinks installed, messages are silently dropped. Level filtering happens *before* `fmt::format` runs, so filtered messages don't pay formatting cost. The default level is `info`.

Built-in sinks cover stdout/stderr (plain or colored) and files. For anything else, subclass `collab::log::sink` (override `write(level, const collab::identifier*, std::string_view)`) and pass `std::make_unique<my_sink>(...)` to `add_sink`. Sinks receive a pointer to the caller's identifier (or `nullptr` for untagged calls), so a custom sink can render attribution, filter by library, or route certain libraries to specific destinations.

If you need to log from outside a library — a script, a one-off binary, a test — the untagged free functions `collab::log::info(...)`, `warn(...)`, etc. still work. They pass no identifier, and sinks emit the bare message.

### Thread safety

`add_sink`, `clear_sinks`, `set_level`, `get_level`, the untagged free functions, and `log_message` all take a single mutex on the shared state. Concurrent log calls serialize through that mutex. The fmt-style template overloads check the level *before* formatting to avoid unnecessary `fmt::format` work; a benign TOCTOU window exists where the level can change between the check and the dispatch, but a slipped/dropped message during a concurrent level change is acceptable for logging.

---

## Errors

`collab::error` is the base for every exception type built on top of `collab-core`. Each library declares a top-level `error` deriving from it (see [Library conventions](#library-conventions)), and specific error types live in a nested `errors` namespace:

```cpp
namespace mylib::errors {
    // No extra fields — inherit the base ctors (raw string or fmt::format_string):
    struct timeout : mylib::error {
        using mylib::error::error;
    };

    // Typed payload — write the ctor, build the message from the data:
    struct connect_failed : mylib::error {
        std::string host;
        int         port;

        connect_failed(std::string h, int p)
            : mylib::error("failed to connect to {}:{}", h, p)
            , host(std::move(h))
            , port(p)
        {}
    };
}
```

Throw site:

```cpp
throw mylib::errors::timeout{"read timed out after {}s", 30};
throw mylib::errors::connect_failed{"example.com", 443};
```

Catch site — standard C++ chain, most-specific first:

```cpp
try { ... }
catch (const mylib::errors::connect_failed& e) { recover(e.host, e.port); }
catch (const mylib::error& e)                  { mylib::log::error("{}", e.what()); }
catch (const collab::error& e)                 { /* any collab lib */ }
catch (const std::exception& e)                { /* belt + suspenders */ }
```

Caught objects carry their typed payload — read members directly (`e.host`, `e.port`), use `e.what()` for the human-readable message.

The same struct is also a valid `std::expected<T, E>` error type — no wrapping, no slicing. Pack alternatives in a `std::variant` when an API can return one of several:

```cpp
std::expected<connection, mylib::errors::connect_failed> connect(...);

using rpc_error = std::variant<mylib::errors::timeout,
                               mylib::errors::connect_failed>;
std::expected<response, rpc_error> rpc_call(...);
```

---

## Publishers

Multi-subscriber, thread-safe pub/sub. RAII subscriptions auto-disconnect on destruction.

```cpp
collab::publisher<int, std::string> on_change;

auto sub = on_change.connect([](int code, std::string_view msg) {
    collab::log::info("change: {} ({})", msg, code);
});

on_change(42, "ready");      // publish — handlers run on this thread
sub.disconnect();            // or just let `sub` fall out of scope
```

`publisher<Args...>` is non-copyable and non-movable — pin it as a member of the owning type. `connect(fn)` takes any callable matching `void(Args...)` and returns a `subscription`. `subscriber_count()` reports the current handler count.

`subscription` is move-only and may safely outlive its `publisher` — `disconnect()` becomes a no-op then. `disconnect()` is idempotent; `connected()` reports current state.

Convention (not enforced): only the owning class publishes.

### Threading contract

- `connect()`, `operator()`, `disconnect()`, and `subscriber_count()` are all safe to call concurrently from any thread on the same `publisher`.
- Handlers run *outside* the publisher's lock. Reentrant and recursive publication is deadlock-free — a handler may freely `connect()`, `disconnect()`, or re-invoke any publisher (including the one it was called from).
- Disconnects during an in-flight publication affect *subsequent* publications, not the current one. Subscribers already captured in the snapshot still fire.
- A `subscription` may safely outlive its `publisher`. `disconnect()` becomes a no-op.

### Caveats

⚠️ **Handlers run on the publishing thread.** "Thread-safe `publisher`" means the *publisher object* is safe under concurrent use — it does **not** mean your handlers are. If two threads publish simultaneously, the same handler may run on both threads at the same time. Handlers that touch shared state must synchronize themselves.

⚠️ **Qt thread affinity.** If a worker thread publishes and a handler touches a `QObject` / `QWidget`, you'll trip Qt's thread-affinity rules (assertion, crash, or scrambled UI). The `publisher` does no marshalling. If you need GUI-thread dispatch, do it inside the handler — e.g. `QMetaObject::invokeMethod(target, fn, Qt::QueuedConnection)`.

⚠️ **Move-only argument types are not supported.** `publisher<std::unique_ptr<T>>` and similar will not compile. Broadcasting requires passing each subscriber its own copy of the arguments, which move-only types can't satisfy. Pass by `const T&` or `std::shared_ptr<T>` instead.

---

## Atomic file writer

Durable, atomic file replacement. Writes go to a sibling temp file; `commit()` flushes to disk and atomically renames temp → target. If the writer is destroyed without `commit()`, the temp is discarded and the target is untouched. If `commit()` returned, the data is on the platter at the target path.

```cpp
namespace collab {
    // Convenience: single blob, one call.
    void atomic_file_write(std::filesystem::path target, std::span<const std::byte> bytes);
    void atomic_file_write(std::filesystem::path target, std::string_view bytes);

    class atomic_file_writer {
    public:
        // Throws `target_read_only` if the existing file is read-only (no temp created).
        // On overwrite, mode/owner of the existing target are preserved onto the new file.
        // A symlink target is replaced with a regular file (rename(2) semantics).
        explicit atomic_file_writer(std::filesystem::path target);

        void write(std::span<const std::byte> bytes);
        void write(std::string_view bytes);

        // POSIX:   fsync(temp) → rename → fsync(parent_dir).
        // Windows: FlushFileBuffers + MoveFileExW(REPLACE_EXISTING | WRITE_THROUGH).
        // Throws on failure.
        void commit();

        // Opt-in: if the rename would cross filesystems, fall back to copy+remove
        // (atomicity guarantee waived). Off by default — throws `cross_filesystem`
        // when off and a cross-fs rename is needed.
        void set_direct_write_fallback(bool enabled) noexcept;
    };
}
```

Free function for a single blob; class form for incremental writes built up in pieces.

### Errors

Every failure throws a type from `collab::errors::atomic_file_write::*`. All leaves derive from `collab::errors::atomic_file_write::error` and carry two fields:

- `std::filesystem::path path` — the user-facing target path
- `int os_error_code` — `errno` on POSIX, `GetLastError()` on Windows, `0` when the failure isn't OS-derived

| Type | Thrown when |
|------|-------------|
| `target_read_only` | The existing target is marked read-only; no temp was created. |
| `create_temp_failed` | Couldn't open the sibling temp file (missing dir, no write permission, disk full). |
| `write_failed` | OS write returned an error or made no progress. |
| `fsync_temp_failed` | Couldn't flush the temp's data to disk before rename. |
| `permission_copy_failed` | Couldn't copy attributes from the existing target onto the new temp. |
| `cross_filesystem` | Rename would cross filesystems and `set_direct_write_fallback(true)` isn't set. |
| `rename_failed` | Atomic rename failed for some other reason (busy file on Windows, target's directory removed mid-operation, etc.). |
| `direct_write_failed` | The fallback copy + remove (after `set_direct_write_fallback(true)`) failed. |
| `fsync_parent_dir_failed` | POSIX only; rename succeeded but the parent-directory metadata sync didn't. |

### Caveats

⚠️ `commit()` must be called explicitly. Destroying the writer without committing is the discard path, not a flush.

---

## Terminal styling

> ⚠️ **Header-only consumers** get the color/style enums and `fg::*` constants.
>
> Streaming them with `operator<<` needs the linked impl.

Streaming manipulators for ANSI colors and styles, scoped under `collab::term`. Output is automatically suppressed when stdout/stderr isn't a TTY, when `NO_COLOR` is set, or when piped.

```cpp
using namespace collab::term;

std::cout << bold << fg::green << "ok " << reset_style << reset_color
          << "build complete\n";
```

Foreground colors live under `collab::term::fg::` — `black`, `red`, `green`, `yellow`, `blue`, `magenta`, `cyan`, `gray`. Styles live directly under `collab::term::` — `bold`, `dim`, `italic`, `underline`, `blink`, `reversed`, `crossed`. Reset with `reset_color` and `reset_style`.

---

## Fixed string

`collab::basic_fixed_string<CharT, N, Traits>` is a literal class wrapping a fixed-size character array, usable as a non-type template parameter. Mirrors [P3094](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/p3094r4.html) `std::basic_fixed_string` so consuming code becomes a `using` alias to `std::` once that lands. `N` is the meaningful length; storage is `CharT[N + 1]`.

The `char` alias is `collab::fixed_string<N>`. Parallel aliases exist for the other character types: `fixed_u8string<N>`, `fixed_u16string<N>`, `fixed_u32string<N>`, `fixed_wstring<N>`.

```cpp
constexpr collab::fixed_string s = "hello";   // fixed_string<5>
static_assert(s.size == 5);
static_assert(s == "hello");

template <collab::basic_fixed_string Tag>
struct tagged {
    static constexpr std::string_view name() { return Tag.view(); }
};

using greeting = tagged<"hello">;
```

Implicit conversion to `std::string_view` covers most consumption patterns. `std::hash`, `std::formatter`, and `fmt::formatter` specializations let it interoperate with the standard library and fmt directly — see the [compiler note](#compilers-requiring-include-alongside-import) for the import-only path on VS2022 and GCC 14.

---

## License

[BSD Zero Clause](LICENSE) (SPDX: `0BSD`).
