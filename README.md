# collab-core 🏴‍☠️

Foundational C++23 library for the **Collab** stack. Provides identity and manifest types for libraries, semantic versioning, structured logging with per-library attribution, a thread-safe event primitive, and ANSI terminal styling.

Requires a C++23 toolchain with module support.

---

## Table of contents

- [Getting started](#getting-started)
- [Library conventions](#library-conventions)
- [Identity and manifest](#identity-and-manifest)
- [Semantic versioning](#semantic-versioning)
- [Logging](#logging)
- [Events](#events)
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

A library built on `collab.core` is expected to expose four names from its top-level namespace: **`manifest`**, **`identity`**, **`version`**, and **`log`**. Declare them once at the library's root — usually in a single partition or header — and reuse them everywhere:

```cpp
namespace collab::net {
    inline const collab::core::manifest manifest{
        .identity = {
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

    inline const auto& identity = manifest.identity;
    inline const auto& version  = manifest.version;

    using log = collab::log::logger<identity>;
}
```

Two references and a using-alias — the `identity` and `version` views never drift from the manifest, and the logger is bound to this library's identity at compile time (no per-call objects, no implicit context).

Now any code in the library can just log:

```cpp
namespace collab::net {
    void connect(std::string_view host, int port) {
        log::info("connecting to {}:{}", host, port);
        log::warn("retry attempt {}", n);
    }
}
```

Sinks installed by the app see the identity and decide how to render it — console sinks prefix with `[Collab Net]` (display name), file sinks prefix with `[com.mrowrpurr.collab-net]` (bundle ID, for grep). Libraries don't deal with sinks; they just log.

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

## Identity and manifest

`collab::core::identity` carries the bits a library uses to derive paths, bundle IDs, and folder names — app slug + display name, org slug + display name, and the reverse-DNS root segment. `manifest` composes an identity with descriptive metadata: version, description, authors, license.

A library that hasn't grown descriptive metadata yet can declare just an identity:

```cpp
collab::core::identity ident{
    .app_id   = "collab-core",
    .app_name = "Collab Core",
    .org_id   = "mrowrpurr",
    .org_name = "Mrowr Purr",
    .tld      = "com",
};
assert(ident.bundle_id() == "com.mrowrpurr.collab-core");
```

`identity::bundle_id()` produces the reverse-DNS form `tld.org_id.app_id`.

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

Built-in sinks cover stdout/stderr (plain or colored) and files. For anything else, subclass `collab::log::sink` (override `write(level, const collab::core::identity*, std::string_view)`) and pass `std::make_unique<my_sink>(...)` to `add_sink`. Sinks receive a pointer to the caller's identity (or `nullptr` for untagged calls), so a custom sink can render attribution, filter by library, or route certain libraries to specific destinations.

If you need to log from outside a library — a script, a one-off binary, a test — the untagged free functions `collab::log::info(...)`, `warn(...)`, etc. still work. They pass no identity, and sinks emit the bare message.

See [`docs/logging.md`](docs/logging.md) for additional notes.

---

## Events

Multi-subscriber, thread-safe event. RAII subscriptions auto-disconnect on destruction.

```cpp
collab::core::event<int, std::string> changed;

auto sub = changed.connect([](int code, std::string_view msg) {
    collab::log::info("changed: {} ({})", msg, code);
});

changed(42, "ready");        // invoke — handlers run on this thread
sub.disconnect();            // or just let `sub` fall out of scope
```

`event<Args...>` is non-copyable and non-movable — pin it as a member of the type that owns it. `connect(fn)` takes any callable matching `void(Args...)` and returns a `subscription`; the handler stays alive until that subscription is dropped or `disconnect()`-ed. Invoke via `operator()` (not `emit` — Qt steals that name as a preprocessor macro). `subscriber_count()` reports the current handler count.

`subscription` is move-only and may safely outlive its `event` — disconnect becomes a no-op once the event is destroyed. `disconnect()` is idempotent; `connected()` reports current state.

Convention (not enforced): only the owning class invokes the event.

### Threading contract

- `connect()`, `operator()`, `disconnect()`, and `subscriber_count()` are all safe to call concurrently from any thread on the same `event`.
- Handlers run *outside* the event's lock. Reentrant and recursive emission is deadlock-free — a handler may freely `connect()`, `disconnect()`, or re-invoke the event (including the same `event`).
- Disconnects during an in-flight emission affect *subsequent* emissions, not the current one.
- A `subscription` may safely outlive its `event`. Disconnect becomes a no-op.

### Caveats

⚠️ **Handlers run on the invoking thread.** "Thread-safe `event`" means the *event* object is safe under concurrent use — it does **not** mean your handlers are. If two threads invoke the event simultaneously, the same handler may run on both threads at the same time. Handlers that touch shared state must synchronize themselves.

⚠️ **Qt thread affinity.** If a worker thread invokes the event and a handler touches a `QObject` / `QWidget`, you'll trip Qt's thread-affinity rules (assertion, crash, or scrambled UI). The `event` does no marshalling. If you need GUI-thread dispatch, do it inside the handler — e.g. `QMetaObject::invokeMethod(target, fn, Qt::QueuedConnection)`.

⚠️ **Move-only argument types are not supported.** `event<std::unique_ptr<T>>` and similar will not compile. Multi-broadcast requires passing each handler its own copy of the arguments, which move-only types can't satisfy. Pass by `const T&` or `std::shared_ptr<T>` instead.

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
