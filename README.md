# collab-core 🏴‍☠️

Foundational C++23 library for the **Collab** stack. Provides semantic versioning, structured logging, terminal styling, and a thread-safe signal/slot primitive.

Requires a C++23 toolchain with module support.

```cpp
import collab.core;

int main() {
    collab::log::add_sink(collab::log::make_stdout_color_sink());
    collab::log::info("collab.core {}", collab::core::version.to_string());
}
```

---

## Table of contents

- [Getting started](#getting-started)
- [Semantic versioning](#semantic-versioning)
- [Project manifest](#project-manifest)
- [Logging](#logging)
- [Terminal styling](#terminal-styling)
- [Signals](#signals)
- [License](#license)

---

## Getting started

One import brings in everything:

```cpp
import collab.core;
```

The library's own version is exposed as a `semver` constant:

```cpp
collab::core::version  // semver{1, 0, 0}
```

---

## Semantic versioning

`collab::core::semver` is an aggregate holding `major`, `minor`, `patch`, plus optional `pre_release` and `build` strings. Three-way comparison follows [SemVer 2.0](https://semver.org); the `build` field is ignored (per §10). `to_string()` produces the canonical `MAJOR.MINOR.PATCH[-PRE][+BUILD]` form.

```cpp
collab::core::semver v{1, 2, 0, "rc.1"};
assert(v < collab::core::semver{1, 2, 0});         // rc.1 precedes the release
assert(v.to_string() == "1.2.0-rc.1");
```

---

## Project manifest

`collab::core::identity` and `collab::core::manifest` describe a project's identity and its descriptive metadata. `identity` carries the bits used to derive paths and bundle IDs — app slug + display name, org slug + display name, and the reverse-DNS root segment. `manifest` composes `identity` with version, description, authors, and license.

`identity` on its own is enough when you only need the path-deriving bits (config folder resolution, for instance):

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

Reach for `manifest` when you also want descriptive metadata:

```cpp
collab::core::manifest m{
    .identity    = ident,
    .version     = {1, 0, 0},
    .description = "Foundational C++23 library.",
    .authors     = {"Mrowr Purr"},
    .license     = "0BSD",
};
```

`description` and `license` are `std::optional<std::string>` — absence is distinct from an explicitly empty value. `authors` is a plain `std::vector<std::string>` because an empty vector already means "zero authors" with no ambiguity.

---

## Logging

Library code just calls `collab::log::info(...)` (or `warn`, `error`, etc.) and gets on with its day — no sinks, no setup, no plumbing. The app or binary at the top of the stack installs sinks once at startup; everything underneath stays oblivious to where output ends up. With no sinks installed, messages are silently dropped. Level filtering happens *before* `fmt::format` runs, so filtered messages don't pay formatting cost. The default level is `info`.

**Library code — log freely:**

```cpp
import collab.core;

void connect(std::string_view host, int port) {
    collab::log::info("connecting to {}:{}", host, port);
    // ...
    collab::log::warn("retry attempt {} after {}ms", n, elapsed.count());
}
```

**App code — install sinks at startup:**

```cpp
int main() {
    collab::log::add_sink(collab::log::make_stdout_color_sink());
    collab::log::add_sink(collab::log::make_file_sink("app.log"));
    collab::log::set_level(collab::log::level::debug);
    // ...
}
```

Built-in sinks cover stdout/stderr (plain or colored) and files. For anything else, subclass `collab::log::sink` (override `write(level, std::string_view)`) and pass a `std::make_unique<my_sink>(...)` to `add_sink`.

See [`docs/logging.md`](docs/logging.md) for additional notes.

---

## Terminal styling

Streaming manipulators for ANSI colors and styles. Output is automatically suppressed when stdout/stderr is not a TTY, when `NO_COLOR` is set, or when piped.

### Enums

```cpp
enum class collab::term::color {
    black, red, green, yellow, blue, magenta, cyan, gray, reset
};

enum class collab::term::style {
    bold, dim, italic, underline, blink, reversed, crossed, reset
};

std::ostream& operator<<(std::ostream&, color);
std::ostream& operator<<(std::ostream&, style);
```

### Convenience constants (for `using namespace collab::term;`)

```cpp
namespace collab::term::fg {
    black, red, green, yellow, blue, magenta, cyan, gray
}

reset_color, reset_style
bold, dim, italic, underline, blink, reversed, crossed
```

```cpp
using namespace collab::term;
std::cout << bold << fg::green << "ok " << reset_style << reset_color
          << "build complete\n";
```

---

## Signals

Multi-subscriber, thread-safe signal/slot. RAII subscriptions auto-disconnect on destruction.

```cpp
collab::core::signal<int, std::string> changed;

auto sub = changed.connect([](int code, std::string_view msg) {
    collab::log::info("changed: {} ({})", msg, code);
});

changed(42, "ready");        // invoke — handlers run on this thread
sub.disconnect();            // or just let `sub` fall out of scope
```

`signal<Args...>` is non-copyable and non-movable — pin it as a member of the type that owns the event. `connect(fn)` takes any callable matching `void(Args...)` and returns a `subscription`; the handler stays alive until that subscription is dropped or `disconnect()`-ed. Invoke via `operator()` (not `emit` — Qt steals that name as a preprocessor macro). `subscriber_count()` reports the current handler count.

`subscription` is move-only and may safely outlive its `signal` — disconnect becomes a no-op once the signal is destroyed. `disconnect()` is idempotent; `connected()` reports current state.

Convention (not enforced): only the owning class invokes the signal — same rule as Qt, Boost.Signals2, sigc++.

### Threading contract

- `connect()`, `operator()`, `disconnect()`, and `subscriber_count()` are all safe to call concurrently from any thread on the same `signal`.
- Handlers run *outside* the signal's lock. Reentrant and recursive emission is deadlock-free — a handler may freely `connect()`, `disconnect()`, or re-invoke the signal (including the same `signal`).
- Disconnects during an in-flight emission affect *subsequent* emissions, not the current one.
- A `subscription` may safely outlive its `signal`. Disconnect becomes a no-op.

### Caveats

⚠️ **Handlers run on the invoking thread.** "Thread-safe `signal`" means the *signal* object is safe under concurrent use — it does **not** mean your handlers are. If two threads invoke the signal simultaneously, the same handler may run on both threads at the same time. Handlers that touch shared state must synchronize themselves.

⚠️ **Qt thread affinity.** If a worker thread invokes the signal and a handler touches a `QObject` / `QWidget`, you'll trip Qt's thread-affinity rules (assertion, crash, or scrambled UI). The `signal` does no marshalling. If you need GUI-thread dispatch, do it inside the handler — e.g. `QMetaObject::invokeMethod(target, fn, Qt::QueuedConnection)`.

⚠️ **Move-only argument types are not supported.** `signal<std::unique_ptr<T>>` and similar will not compile. Multi-broadcast requires passing each handler its own copy of the arguments, which move-only types can't satisfy. Pass by `const T&` or `std::shared_ptr<T>` instead.

---

## License

[BSD Zero Clause](LICENSE) (SPDX: `0BSD`).
