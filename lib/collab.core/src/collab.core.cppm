module;

#include <filesystem>
#include <memory>
#include <string_view>
#include <utility>

#include <fmt/format.h>

#include "collab/detail/types.hpp"

export module collab.core;

export import :signal;
export import :term;

// ── Value types (single source of truth) ──────────────────────────────────
//
// Re-export the value types and the `level`/`sink` log types from the header.
// The using-declarations bind the names in the module's purview to the same
// entities the GMF-included header attaches to the global module — so both
// `import collab.core;` and `#include <collab/core.hpp>` reach one entity each.

namespace collab::core {
    export using ::collab::core::identity;
    export using ::collab::core::manifest;
    export using ::collab::core::semver;
    export using ::collab::core::version;
}

namespace collab::log {

export using ::collab::log::level;
export using ::collab::log::sink;

// ── Logging API (module-attached non-template overloads) ──────────────────
//
// Declared here as module-attached entities, defined in lib/collab.core/src/
// log.cpp. They share state with the header's inline equivalents through the
// inline `detail::state()` singleton in <collab/detail/state.hpp>, which both
// paths #include. Their bodies must live separately from the header's inline
// bodies because cross-attachment redeclaration of the same name is ill-formed
// ([basic.link]/9) — that's the unavoidable function-body duplication.

export void set_level(level l);
export level get_level();
export void add_sink(std::unique_ptr<sink> snk);
export void clear_sinks();
export void log_message(level lvl, const collab::core::identity* id, std::string_view msg);

export void trace   (std::string_view msg);
export void debug   (std::string_view msg);
export void info    (std::string_view msg);
export void warn    (std::string_view msg);
export void error   (std::string_view msg);
export void critical(std::string_view msg);

export void trace_with   (const collab::core::identity& id, std::string_view msg);
export void debug_with   (const collab::core::identity& id, std::string_view msg);
export void info_with    (const collab::core::identity& id, std::string_view msg);
export void warn_with    (const collab::core::identity& id, std::string_view msg);
export void error_with   (const collab::core::identity& id, std::string_view msg);
export void critical_with(const collab::core::identity& id, std::string_view msg);

// ── spdlog-backed sink factories ──────────────────────────────────────────
// Defined in lib/collab.core/src/log.cpp; not declared in <collab/core.hpp>
// so header-only consumers don't accumulate a link contract on spdlog.

export std::unique_ptr<sink> make_stdout_sink();
export std::unique_ptr<sink> make_stdout_color_sink();
export std::unique_ptr<sink> make_stderr_sink();
export std::unique_ptr<sink> make_stderr_color_sink();
export std::unique_ptr<sink> make_file_sink(std::filesystem::path path);

}  // namespace collab::log

// ── fmt-style variadic templates + logger<I> class template ───────────────
// Module-attached. Same source as the header-only path; #included inside an
// `export { }` block so consumers of `import collab.core;` see the templates.

export {
#include "collab/detail/log_inline.hpp"
}
