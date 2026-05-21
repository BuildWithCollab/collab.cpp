# collab-core 🏴‍☠️

Foundational C++23 library for the **Collab** stack. Provides identifier and manifest types for libraries, semantic versioning, structured logging with per-library attribution, a thread-safe multi-subscriber publisher, and ANSI terminal styling.

Requires a C++23 toolchain with module support.

---

## Table of contents

- [Getting started](#getting-started)
- [Library conventions](#library-conventions)
- [Identifier and manifest](#identifier-and-manifest)
- [Semantic versioning](#semantic-versioning)
- [Logging](#logging)
- [Errors](#errors)
- [Publishers](#publishers)
- [Terminal styling](#terminal-styling)
- [License](#license)

---

## Getting started

One import brings in everything:

```cpp
import collab.core;
```

---

## Library conventions

A library built on `collab.core` is expected to expose five names from its top-level namespace: **`manifest`**, **`identifier`**, **`version`**, **`log`**, and **`error`**. Declare them once at the library's root — usually in a single partition or header — and reuse them everywhere:

```cpp
namespace collab::net {
    inline const collab::core::manifest manifest{
        .identifier = {
            .app_id   = "collab-net",
            .app_name = "Collab Net",
            .org_id   = "mrowrpurr",
            .org_name = "Mrowr Purr",
            .tld      = "com",
        },
        .version     = {0, 1, 0},
        .description = "Networking layer.",
        .authors     = {"Mrowr Purr"},
        .license     = "0BSD",
    };

    inline const auto& identifier = manifest.identifier;
    inline const auto& version    = manifest.version;

    using log = collab::log::logger<identifier>;

    struct error : collab::core::error {
        using collab::core::error::error;
    };
}
```

The `identifier` and `version` views never drift from the manifest; the logger is bound to this library's identifier at compile time; and `error` anchors the library's exception hierarchy (specific error types live in a nested `errors` namespace — see [Errors](#errors)).

Now any code in the library can just log:

```cpp
namespace collab::net {
    void connect(std::string_view host, int port) {
        log::info("connecting to {}:{}", host, port);
        log::warn("retry attempt {}", n);
    }
}
```

Sinks installed by the app see the identifier and decide how to render it — console sinks prefix with `[Collab Net]` (display name), file sinks prefix with `[com.mrowrpurr.collab-net]` (bundle ID, for grep). Libraries don't deal with sinks; they just log.

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

`collab::core::identifier` carries the bits a library uses to derive paths, bundle IDs, and folder names — app slug + display name, org slug + display name, and the reverse-DNS root segment. `manifest` composes an identifier with descriptive metadata: version, description, authors, license.

A library that hasn't grown descriptive metadata yet can declare just an identifier:

```cpp
collab::core::identifier ident{
    .app_id   = "collab-core",
    .app_name = "Collab Core",
    .org_id   = "mrowrpurr",
    .org_name = "Mrowr Purr",
    .tld      = "com",
};
assert(ident.bundle_id() == "com.mrowrpurr.collab-core");
```

`identifier::bundle_id()` produces the reverse-DNS form `tld.org_id.app_id`.

A `manifest` adds version, description, authors, and license on top — see the example in [Library conventions](#library-conventions). `description` and `license` are `std::optional<std::string>`; absence is distinct from an explicitly empty value. `authors` is a plain `std::vector<std::string>` because an empty vector already means "zero authors" with no ambiguity.

---

## Semantic versioning

`collab::core::semver` is an aggregate holding `major`, `minor`, `patch`, plus optional `pre_release` and `build` strings. Three-way comparison follows [SemVer 2.0](https://semver.org); the `build` field is ignored (per §10). `to_string()` produces the canonical `MAJOR.MINOR.PATCH[-PRE][+BUILD]` form.

```cpp
collab::core::semver v{1, 2, 0, "rc.1"};
assert(v < collab::core::semver{1, 2, 0});         // rc.1 precedes the release
assert(v.to_string() == "1.2.0-rc.1");
```

---

## Logging

Library code never deals with sinks — it just calls `log::info(...)` (see [Library conventions](#library-conventions)). The **application** at the top of the stack is responsible for installing sinks, choosing the level, and deciding where output ends up.

**App code — install sinks at startup:**

```cpp
int main() {
    collab::log::add_sink(collab::log::make_stdout_color_sink());
    collab::log::add_sink(collab::log::make_file_sink("app.log"));
    collab::log::set_level(collab::log::level::debug);
    // ...
}
```

With no sinks installed, messages are silently dropped. Level filtering happens *before* `fmt::format` runs, so filtered messages don't pay formatting cost. The default level is `info`.

Built-in sinks cover stdout/stderr (plain or colored) and files. For anything else, subclass `collab::log::sink` (override `write(level, const collab::core::identifier*, std::string_view)`) and pass `std::make_unique<my_sink>(...)` to `add_sink`. Sinks receive a pointer to the caller's identifier (or `nullptr` for untagged calls), so a custom sink can render attribution, filter by library, or route certain libraries to specific destinations.

If you need to log from outside a library — a script, a one-off binary, a test — the untagged free functions `collab::log::info(...)`, `warn(...)`, etc. still work. They pass no identifier, and sinks emit the bare message.

See [`docs/logging.md`](docs/logging.md) for additional notes.

---

## Errors

`collab::core::error` is the base for every exception type in the Collab stack. Each library declares a top-level `error` deriving from it (see [Library conventions](#library-conventions)), and specific error types live in a nested `errors` namespace:

```cpp
namespace collab::net::errors {
    // No extra fields — inherit the base ctors (raw string or fmt::format_string):
    struct timeout : collab::net::error {
        using collab::net::error::error;
    };

    // Typed payload — write the ctor, build the message from the data:
    struct connect_failed : collab::net::error {
        std::string host;
        int         port;

        connect_failed(std::string h, int p)
            : collab::net::error("failed to connect to {}:{}", h, p)
            , host(std::move(h))
            , port(p)
        {}
    };
}
```

Throw site:

```cpp
throw collab::net::errors::timeout{"read timed out after {}s", 30};
throw collab::net::errors::connect_failed{"example.com", 443};
```

Catch site — standard C++ chain, most-specific first:

```cpp
try { ... }
catch (const collab::net::errors::connect_failed& e) { recover(e.host, e.port); }
catch (const collab::net::error& e)                  { collab::net::log::error("{}", e.what()); }
catch (const collab::core::error& e)                 { /* any collab lib */ }
catch (const std::exception& e)                      { /* belt + suspenders */ }
```

Caught objects carry their typed payload — read members directly (`e.host`, `e.port`), use `e.what()` for the human-readable message.

The same struct is also a valid `std::expected<T, E>` error type — no wrapping, no slicing. Pack alternatives in a `std::variant` when an API can return one of several:

```cpp
std::expected<connection, collab::net::errors::connect_failed> connect(...);

using rpc_error = std::variant<collab::net::errors::timeout,
                               collab::net::errors::connect_failed>;
std::expected<response, rpc_error> rpc_call(...);
```

---

## Publishers

Multi-subscriber, thread-safe pub/sub. RAII subscriptions auto-disconnect on destruction.

```cpp
collab::core::publisher<int, std::string> on_change;

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

## Terminal styling

Streaming manipulators for ANSI colors and styles. Output is automatically suppressed when stdout/stderr is not a TTY, when `NO_COLOR` is set, or when piped.

```cpp
using namespace collab::term;
std::cout << bold << fg::green << "ok " << reset_style << reset_color
          << "build complete\n";
```

Colors: `black`, `red`, `green`, `yellow`, `blue`, `magenta`, `cyan`, `gray`. Styles: `bold`, `dim`, `italic`, `underline`, `blink`, `reversed`, `crossed`. Use `reset_color` / `reset_style` to clear. Foreground constants live under `collab::term::fg`.

---

## License

[BSD Zero Clause](LICENSE) (SPDX: `0BSD`).
