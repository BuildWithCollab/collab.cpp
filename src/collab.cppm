module;

#include "collab_toggle.hpp"

#ifdef COLLAB_TOGGLE_BROKEN
#include <string_view>
#else
#include <collab/detail/log_decls.hpp>
#endif

export module collab;

#ifdef COLLAB_TOGGLE_BROKEN

export namespace collab::log {

enum class level { trace, debug, info, warn, error, critical, off };

void clear_sinks();
void set_level(level);
level get_level();

void trace   (std::string_view);
void debug   (std::string_view);
void info    (std::string_view);
void warn    (std::string_view);
void error   (std::string_view);
void critical(std::string_view);

}

export namespace collab::log::detail {

struct log_state { level current = level::info; };

}

#else

export namespace collab::log {

using ::collab::log::level;
using ::collab::log::clear_sinks;
using ::collab::log::set_level;
using ::collab::log::get_level;
using ::collab::log::trace;
using ::collab::log::debug;
using ::collab::log::info;
using ::collab::log::warn;
using ::collab::log::error;
using ::collab::log::critical;

}

#endif
