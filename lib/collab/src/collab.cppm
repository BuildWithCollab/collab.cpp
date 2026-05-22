module;

#include <filesystem>
#include <memory>

#include <collab/log.hpp>

export module collab;

export import :term;

namespace collab::log {

// spdlog-backed sink factories. Defined in lib/collab/src/log.cpp.
// Header-only consumers reach the log API itself through <collab/log.hpp>
// from `collab-hpp`; this module adds the sink implementations that need
// to link against spdlog.

export std::unique_ptr<sink> make_stdout_sink();
export std::unique_ptr<sink> make_stdout_color_sink();
export std::unique_ptr<sink> make_stderr_sink();
export std::unique_ptr<sink> make_stderr_color_sink();
export std::unique_ptr<sink> make_file_sink(std::filesystem::path path);

}  // namespace collab::log
