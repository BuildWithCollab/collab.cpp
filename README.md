# collab 🏴‍☠️

The static-link dependency floor for the **Collab** stack. Every non-header-only Collab project pulls this in to get a guaranteed baseline: spdlog-backed log sinks today, more as the floor grows.

Built on [`collab-hpp`](https://github.com/BuildWithCollab/collab.hpp) — pulls it in transitively, so consumers get the full header-only API (identifier, manifest, semver, error, publisher, log interface) for free.

Requires a C++23 toolchain with module support.

---

## Consuming

```lua
add_repositories("BuildWithCollab https://github.com/BuildWithCollab/Packages")
add_requires("collab")

target("myapp")
    add_packages("collab")
```

```cpp
import collab;
#include <collab/log.hpp>   // API lives in collab-hpp
```

`import collab;` exposes the spdlog-backed sink factories. The log API itself (`add_sink`, `set_level`, `logger<I>`, free functions) lives in `<collab/log.hpp>` from `collab-hpp` — see its [README](https://github.com/BuildWithCollab/collab.hpp) for usage.

---

## Sink factories

```cpp
namespace collab::log {
    std::unique_ptr<sink> make_stdout_sink();
    std::unique_ptr<sink> make_stdout_color_sink();
    std::unique_ptr<sink> make_stderr_sink();
    std::unique_ptr<sink> make_stderr_color_sink();
    std::unique_ptr<sink> make_file_sink(std::filesystem::path path);
}
```

Console sinks render the caller's identifier using its display name (`[Collab Net]`). File sinks use the bundle ID (`[com.mrowrpurr.collab-net]`) — better for grepping logs after the fact.

```cpp
int main() {
    collab::log::add_sink(collab::log::make_stdout_color_sink());
    collab::log::add_sink(collab::log::make_file_sink("app.log"));
    collab::log::set_level(collab::log::level::debug);
    // ...
}
```

---

## License

[BSD Zero Clause](LICENSE) (SPDX: `0BSD`).
