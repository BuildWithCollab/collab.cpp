#pragma once

#include <collab/detail/log_decls.hpp>

namespace collab::log::detail {

inline log_state g_state{};

}  // namespace collab::log::detail

namespace collab::log {

inline void clear_sinks()             {}
inline void set_level(level l)        { detail::g_state.current = l; }
inline level get_level()              { return detail::g_state.current; }

inline void trace   (std::string_view) {}
inline void debug   (std::string_view) {}
inline void info    (std::string_view) {}
inline void warn    (std::string_view) {}
inline void error   (std::string_view) {}
inline void critical(std::string_view) {}

}  // namespace collab::log
