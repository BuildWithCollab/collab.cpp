module;

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

module collab.core;

import :log;
import :manifest;

namespace collab::log {

namespace {

    spdlog::level::level_enum to_spdlog_level(level l) {
        switch (l) {
            case level::trace:    return spdlog::level::trace;
            case level::debug:    return spdlog::level::debug;
            case level::info:     return spdlog::level::info;
            case level::warn:     return spdlog::level::warn;
            case level::error:    return spdlog::level::err;
            case level::critical: return spdlog::level::critical;
            case level::off:      return spdlog::level::off;
        }
        return spdlog::level::info;
    }

    constexpr auto logger_prefix_stdout = "collab_stdout_";
    constexpr auto logger_prefix_stderr = "collab_stderr_";
    constexpr auto logger_prefix_file   = "collab_file_";
    constexpr auto log_pattern          = "%v";

    struct log_state {
        level                               current_level = level::info;
        std::vector<std::unique_ptr<sink>>   sinks;
        std::vector<std::string>             logger_names;
        std::mutex                           mtx;
    };

    log_state& state() {
        static log_state s;
        return s;
    }

    void register_logger(std::shared_ptr<spdlog::logger>& logger, const std::string& name) {
        logger->set_level(spdlog::level::trace);  // collab layer is the sole gatekeeper
        logger->set_pattern(log_pattern);
        auto& s = state();
        std::lock_guard lock(s.mtx);
        s.logger_names.push_back(name);
    }

    // Console sinks render identity using the display name — humans reading
    // a terminal want "[Collab Net]" not "[com.mrowrpurr.collab-net]".
    void emit_console(spdlog::logger& logger,
                      level lvl,
                      const collab::core::identity* id,
                      std::string_view msg) {
        if (id)
            logger.log(to_spdlog_level(lvl), "[{}] {}", id->app_name, msg);
        else
            logger.log(to_spdlog_level(lvl), "{}", msg);
    }

    // ── spdlog-backed stdout sinks ──────────────────────────────────

    class stdout_sink final : public sink {
    public:
        stdout_sink() {
            auto name = logger_prefix_stdout + std::to_string(++counter_);
            logger_ = spdlog::stdout_logger_mt(name);
            register_logger(logger_, name);
        }

        void write(level lvl, const collab::core::identity* id, std::string_view msg) override {
            emit_console(*logger_, lvl, id, msg);
        }

    private:
        std::shared_ptr<spdlog::logger> logger_;
        static inline std::atomic<int> counter_{0};
    };

    class stdout_color_sink final : public sink {
    public:
        stdout_color_sink() {
            auto name = logger_prefix_stdout + std::to_string(++counter_);
            logger_ = spdlog::stdout_color_mt(name);
            register_logger(logger_, name);
        }

        void write(level lvl, const collab::core::identity* id, std::string_view msg) override {
            emit_console(*logger_, lvl, id, msg);
        }

    private:
        std::shared_ptr<spdlog::logger> logger_;
        static inline std::atomic<int> counter_{0};
    };

    // ── spdlog-backed stderr sinks ──────────────────────────────────

    class stderr_sink final : public sink {
    public:
        stderr_sink() {
            auto name = logger_prefix_stderr + std::to_string(++counter_);
            logger_ = spdlog::stderr_logger_mt(name);
            register_logger(logger_, name);
        }

        void write(level lvl, const collab::core::identity* id, std::string_view msg) override {
            emit_console(*logger_, lvl, id, msg);
        }

    private:
        std::shared_ptr<spdlog::logger> logger_;
        static inline std::atomic<int> counter_{0};
    };

    class stderr_color_sink final : public sink {
    public:
        stderr_color_sink() {
            auto name = logger_prefix_stderr + std::to_string(++counter_);
            logger_ = spdlog::stderr_color_mt(name);
            register_logger(logger_, name);
        }

        void write(level lvl, const collab::core::identity* id, std::string_view msg) override {
            emit_console(*logger_, lvl, id, msg);
        }

    private:
        std::shared_ptr<spdlog::logger> logger_;
        static inline std::atomic<int> counter_{0};
    };

    // ── spdlog-backed file sink ─────────────────────────────────────
    //
    // File sinks render identity using the bundle ID — "[com.mrowrpurr.collab-net]"
    // is what you want for grepping log files long after the fact.

    class file_sink final : public sink {
    public:
        explicit file_sink(std::filesystem::path path) {
            auto name = logger_prefix_file + std::to_string(++counter_);
            logger_ = spdlog::basic_logger_mt(name, path.string(), false);
            register_logger(logger_, name);
            logger_->flush_on(spdlog::level::trace);
        }

        void write(level lvl, const collab::core::identity* id, std::string_view msg) override {
            if (id)
                logger_->log(to_spdlog_level(lvl), "[{}] {}", id->bundle_id(), msg);
            else
                logger_->log(to_spdlog_level(lvl), "{}", msg);
        }

    private:
        std::shared_ptr<spdlog::logger> logger_;
        static inline std::atomic<int> counter_{0};
    };

}  // namespace

void set_level(level l) {
    auto& s = state();
    std::lock_guard lock(s.mtx);
    s.current_level = l;
}

level get_level() {
    auto& s = state();
    std::lock_guard lock(s.mtx);
    return s.current_level;
}

void add_sink(std::unique_ptr<sink> snk) {
    auto& s = state();
    std::lock_guard lock(s.mtx);
    s.sinks.push_back(std::move(snk));
}

void clear_sinks() {
    auto& s = state();
    std::lock_guard lock(s.mtx);
    s.sinks.clear();
    for (auto& name : s.logger_names)
        spdlog::drop(name);
    s.logger_names.clear();
}

void log_message(level lvl, const collab::core::identity* id, std::string_view msg) {
    auto& s = state();
    std::lock_guard lock(s.mtx);
    if (lvl < s.current_level) return;
    for (auto& snk : s.sinks)
        snk->write(lvl, id, msg);
}

// Untagged: pass nullptr.
void trace(std::string_view msg)    { log_message(level::trace,    nullptr, msg); }
void debug(std::string_view msg)    { log_message(level::debug,    nullptr, msg); }
void info(std::string_view msg)     { log_message(level::info,     nullptr, msg); }
void warn(std::string_view msg)     { log_message(level::warn,     nullptr, msg); }
void error(std::string_view msg)    { log_message(level::error,    nullptr, msg); }
void critical(std::string_view msg) { log_message(level::critical, nullptr, msg); }

// Tagged: pass identity through.
void trace_with(const collab::core::identity& id, std::string_view msg) {
    log_message(level::trace, &id, msg);
}
void debug_with(const collab::core::identity& id, std::string_view msg) {
    log_message(level::debug, &id, msg);
}
void info_with(const collab::core::identity& id, std::string_view msg) {
    log_message(level::info, &id, msg);
}
void warn_with(const collab::core::identity& id, std::string_view msg) {
    log_message(level::warn, &id, msg);
}
void error_with(const collab::core::identity& id, std::string_view msg) {
    log_message(level::error, &id, msg);
}
void critical_with(const collab::core::identity& id, std::string_view msg) {
    log_message(level::critical, &id, msg);
}

std::unique_ptr<sink> make_stdout_sink() {
    return std::make_unique<stdout_sink>();
}

std::unique_ptr<sink> make_stdout_color_sink() {
    return std::make_unique<stdout_color_sink>();
}

std::unique_ptr<sink> make_stderr_sink() {
    return std::make_unique<stderr_sink>();
}

std::unique_ptr<sink> make_stderr_color_sink() {
    return std::make_unique<stderr_color_sink>();
}

std::unique_ptr<sink> make_file_sink(std::filesystem::path path) {
    return std::make_unique<file_sink>(std::move(path));
}

}  // namespace collab::log
