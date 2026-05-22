module;

#include <atomic>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <collab/identifier.hpp>
#include <collab/log.hpp>

module collab;

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

    std::string unique_name(std::string_view prefix) {
        static std::atomic<int> counter{0};
        return std::string(prefix) + std::to_string(++counter);
    }

    // Console sinks render identifier using the display name — humans reading
    // a terminal want "[Collab Net]" not "[com.mrowrpurr.collab-net]".
    void emit_console(spdlog::logger& logger,
                      level lvl,
                      const collab::identifier* id,
                      std::string_view msg) {
        if (id)
            logger.log(to_spdlog_level(lvl), "[{}] {}", id->app_name, msg);
        else
            logger.log(to_spdlog_level(lvl), "{}", msg);
    }

    // Base for spdlog-backed sinks. Owns the spdlog registration so dropping
    // the sink also drops its named logger from spdlog's global registry —
    // letting `clear_sinks()` (just a vector clear) reclaim spdlog state.
    class spdlog_sink_base : public sink {
    public:
        spdlog_sink_base(std::shared_ptr<spdlog::logger> logger, std::string name)
            : logger_{std::move(logger)}, name_{std::move(name)} {
            logger_->set_level(spdlog::level::trace);  // collab layer is the sole gatekeeper
            logger_->set_pattern(log_pattern);
        }

        ~spdlog_sink_base() override { spdlog::drop(name_); }

    protected:
        std::shared_ptr<spdlog::logger> logger_;

    private:
        std::string name_;
    };

    // Factory helper: build the spdlog logger first, then pair it with the
    // name for the base constructor. Doing this through a free function
    // dodges the unspecified argument-evaluation order of `base(make(name),
    // std::move(name))` — the call would otherwise risk moving from `name`
    // before `make(name)` read it, leaving spdlog with an empty string.
    struct spdlog_pair {
        std::shared_ptr<spdlog::logger> logger;
        std::string                     name;
    };

    template<typename MakeFn>
    spdlog_pair make_spdlog_pair(std::string_view prefix, MakeFn&& make) {
        auto name   = unique_name(prefix);
        auto logger = make(name);
        return {std::move(logger), std::move(name)};
    }

    class stdout_sink final : public spdlog_sink_base {
    public:
        stdout_sink() : stdout_sink(make_spdlog_pair(
            logger_prefix_stdout,
            [](const std::string& n) { return spdlog::stdout_logger_mt(n); })) {}
        void write(level lvl, const collab::identifier* id, std::string_view msg) override {
            emit_console(*logger_, lvl, id, msg);
        }
    private:
        explicit stdout_sink(spdlog_pair p)
            : spdlog_sink_base(std::move(p.logger), std::move(p.name)) {}
    };

    class stdout_color_sink final : public spdlog_sink_base {
    public:
        stdout_color_sink() : stdout_color_sink(make_spdlog_pair(
            logger_prefix_stdout,
            [](const std::string& n) { return spdlog::stdout_color_mt(n); })) {}
        void write(level lvl, const collab::identifier* id, std::string_view msg) override {
            emit_console(*logger_, lvl, id, msg);
        }
    private:
        explicit stdout_color_sink(spdlog_pair p)
            : spdlog_sink_base(std::move(p.logger), std::move(p.name)) {}
    };

    class stderr_sink final : public spdlog_sink_base {
    public:
        stderr_sink() : stderr_sink(make_spdlog_pair(
            logger_prefix_stderr,
            [](const std::string& n) { return spdlog::stderr_logger_mt(n); })) {}
        void write(level lvl, const collab::identifier* id, std::string_view msg) override {
            emit_console(*logger_, lvl, id, msg);
        }
    private:
        explicit stderr_sink(spdlog_pair p)
            : spdlog_sink_base(std::move(p.logger), std::move(p.name)) {}
    };

    class stderr_color_sink final : public spdlog_sink_base {
    public:
        stderr_color_sink() : stderr_color_sink(make_spdlog_pair(
            logger_prefix_stderr,
            [](const std::string& n) { return spdlog::stderr_color_mt(n); })) {}
        void write(level lvl, const collab::identifier* id, std::string_view msg) override {
            emit_console(*logger_, lvl, id, msg);
        }
    private:
        explicit stderr_color_sink(spdlog_pair p)
            : spdlog_sink_base(std::move(p.logger), std::move(p.name)) {}
    };

    // File sinks render identifier using the bundle ID — "[com.mrowrpurr.collab-net]"
    // is what you want for grepping log files long after the fact.
    class file_sink final : public spdlog_sink_base {
    public:
        explicit file_sink(std::filesystem::path path)
            : file_sink(make_spdlog_pair(
                logger_prefix_file,
                [&](const std::string& n) {
                    return spdlog::basic_logger_mt(n, path.string(), false);
                })) {
            logger_->flush_on(spdlog::level::trace);
        }

        void write(level lvl, const collab::identifier* id, std::string_view msg) override {
            if (id)
                logger_->log(to_spdlog_level(lvl), "[{}] {}", id->bundle_id(), msg);
            else
                logger_->log(to_spdlog_level(lvl), "{}", msg);
        }

    private:
        file_sink(spdlog_pair p)
            : spdlog_sink_base(std::move(p.logger), std::move(p.name)) {}
    };

}  // namespace

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
