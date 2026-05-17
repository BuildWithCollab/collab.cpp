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

[`fmt`](https://github.com/fmtlib/fmt) is re-exported, so `fmt::format_string` appears in the logging API.

The library's own version is exposed as a `semver` constant:

```cpp
collab::core::version  // semver{1, 0, 0}
```

---

## Semantic versioning

Semantic version per [SemVer 2.0](https://semver.org). Comparison ignores the `build` field (per §10).

```cpp
struct semver {
    int         major = 0;
    int         minor = 0;
    int         patch = 0;
    std::string pre_release;   // e.g. "rc.1"
    std::string build;         // e.g. "exp.sha.5114f85" — ignored in <=>

    std::string to_string() const;

    std::strong_ordering operator<=>(const semver&) const;
    bool                 operator==(const semver&) const;
};
```

```cpp
collab::core::semver v{1, 2, 0, "rc.1"};
assert(v < collab::core::semver{1, 2, 0});         // rc.1 precedes the release
assert(v.to_string() == "1.2.0-rc.1");
```

---

## Logging

Sink-based logging with both plain-string and `fmt`-style variadic overloads. Level filtering happens *before* `fmt::format` runs, so filtered messages don't pay formatting cost.

### Levels

```cpp
enum class collab::log::level { trace, debug, info, warn, error, critical, off };
```

### Configuration

```cpp
void  collab::log::set_level(level l);
level collab::log::get_level();

void  collab::log::add_sink(std::unique_ptr<sink> s);
void  collab::log::clear_sinks();
```

### Built-in sinks

```cpp
std::unique_ptr<sink> make_stdout_sink();
std::unique_ptr<sink> make_stdout_color_sink();
std::unique_ptr<sink> make_stderr_sink();
std::unique_ptr<sink> make_stderr_color_sink();
std::unique_ptr<sink> make_file_sink(std::filesystem::path path);
```

### Custom sinks

```cpp
class collab::log::sink {
public:
    virtual ~sink() = default;
    virtual void write(level lvl, std::string_view msg) = 0;
};
```

Implement and register with `add_sink(std::make_unique<MySink>(...))`.

### Logging

```cpp
// Plain string overloads
collab::log::trace   (std::string_view msg);
collab::log::debug   (std::string_view msg);
collab::log::info    (std::string_view msg);
collab::log::warn    (std::string_view msg);
collab::log::error   (std::string_view msg);
collab::log::critical(std::string_view msg);

// fmt-style variadic overloads
collab::log::info("connected to {} in {}ms", host, elapsed.count());
```

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

Multi-subscriber, thread-safe signal. Subscriptions are RAII tokens that auto-disconnect on destruction. Convention (not enforced): only the owning class invokes the signal — same rule as Qt, Boost.Signals2, sigc++.

Emission is `operator()`, not a member named `emit`. Qt defines `emit` as an empty preprocessor macro (`qtmetamacros.h`), so a member function called `emit` would silently break for any consumer that also pulls in a Qt header. Call syntax sidesteps the collision entirely — `sig(args)` works whether Qt is present or not.

### `Signal<Args...>`

```cpp
template <typename... Args>
class Signal {
public:
    using Handler = std::function<void(Args...)>;

    Signal();
    Signal(const Signal&)            = delete;
    Signal(Signal&&)                 = delete;
    Signal& operator=(const Signal&) = delete;
    Signal& operator=(Signal&&)      = delete;

    [[nodiscard]] Subscription connect(Handler handler);
    void                       operator()(Args... args);
    std::size_t                subscriber_count() const;
};
```

### `Subscription`

Move-only RAII token. Disconnect is automatic on destruction; calling `disconnect()` is also fine.

```cpp
class Subscription {
public:
    Subscription() noexcept = default;
    Subscription(Subscription&&) noexcept;
    Subscription& operator=(Subscription&&) noexcept;
    ~Subscription();

    void disconnect() noexcept;
    bool connected()  const noexcept;
};
```

### Threading contract

- `connect()`, `operator()`, `disconnect()`, and `subscriber_count()` are all safe to call concurrently from any thread on the same `Signal`.
- Handlers run *outside* the signal's lock. Reentrant and recursive emission is deadlock-free — a handler may freely `connect()`, `disconnect()`, or re-invoke the signal (including the same `Signal`).
- Disconnects during an in-flight emission affect *subsequent* emissions, not the current one.
- A `Subscription` may safely outlive its `Signal`. Disconnect becomes a no-op.

### Caveats

⚠️ **Handlers run on the emitting thread.** "Thread-safe `Signal`" means the *signal* object is safe under concurrent use — it does **not** mean your handlers are. If two threads invoke the signal simultaneously, the same handler may run on both threads at the same time. Handlers that touch shared state must synchronize themselves.

⚠️ **Qt thread affinity.** If a worker thread emits and a handler touches a `QObject` / `QWidget`, you'll trip Qt's thread-affinity rules (assertion, crash, or scrambled UI). The `Signal` does no marshalling. If you need GUI-thread dispatch, do it inside the handler — e.g. `QMetaObject::invokeMethod(target, fn, Qt::QueuedConnection)`.

⚠️ **Move-only argument types are not supported.** `Signal<std::unique_ptr<T>>` and similar will not compile. Multi-broadcast requires passing each handler its own copy of the arguments, which move-only types can't satisfy. Pass by `const T&` or `std::shared_ptr<T>` instead.

### Example

```cpp
collab::core::Signal<int, std::string> changed;

auto sub = changed.connect([](int code, std::string_view msg) {
    collab::log::info("changed: {} ({})", msg, code);
});

changed(42, "ready");

// Disconnect explicitly, or just let `sub` go out of scope.
sub.disconnect();
```

---

## License

[BSD Zero Clause](LICENSE) (SPDX: `0BSD`).
