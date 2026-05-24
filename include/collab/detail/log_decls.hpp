#pragma once

#include <string_view>

namespace collab::log {

enum class level { trace, debug, info, warn, error, critical, off };

}  // namespace collab::log

namespace collab::log::detail {

struct log_state {
    level current = level::info;
};

}  // namespace collab::log::detail

namespace collab::log {

void clear_sinks();
void set_level(level);
level get_level();

void trace   (std::string_view);
void debug   (std::string_view);
void info    (std::string_view);
void warn    (std::string_view);
void error   (std::string_view);
void critical(std::string_view);

}  // namespace collab::log
