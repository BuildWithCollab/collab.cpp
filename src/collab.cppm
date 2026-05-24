module;

#include <collab/detail/log_decls.hpp>

export module collab;

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
