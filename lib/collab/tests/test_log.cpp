#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include <collab/log.hpp>

import collab;

using namespace collab::log;

struct log_fixture {
    log_fixture() {
        clear_sinks();
        set_level(level::trace);
    }
    ~log_fixture() {
        clear_sinks();
        set_level(level::info);
    }
};

// Color output uses Win32 console API on Windows (wincolor_sink), not ANSI
// escapes. The color difference is untestable via fd capture — console
// attribute calls don't appear in the byte stream. These tests confirm the
// sink can be installed and that the level filter still routes correctly.

TEST_CASE("stdout sink can be created", "[log][sink]") {
    log_fixture fix;
    add_sink(make_stdout_sink());
    set_level(level::warn);
    warn("stdout test warning");
    error("stdout test error");
}

TEST_CASE("stdout color sink can be created", "[log][sink]") {
    log_fixture fix;
    add_sink(make_stdout_color_sink());
    set_level(level::warn);
    warn("stdout color warning");
    error("stdout color error");
}

TEST_CASE("stderr sink can be created", "[log][sink]") {
    log_fixture fix;
    add_sink(make_stderr_sink());
    set_level(level::warn);
    warn("stderr test warning");
    error("stderr test error");
    critical("stderr test critical");
}

TEST_CASE("stderr color sink can be created", "[log][sink]") {
    log_fixture fix;
    add_sink(make_stderr_color_sink());
    set_level(level::warn);
    warn("stderr color warning");
    error("stderr color error");
    critical("stderr color critical");
}

TEST_CASE("file sink writes to disk", "[log][sink]") {
    log_fixture fix;

    auto path = std::filesystem::temp_directory_path() / "collab_test_log.txt";
    std::filesystem::remove(path);

    add_sink(make_file_sink(path));
    info("line one");
    warn("line two");
    clear_sinks();

    std::ifstream in(path);
    REQUIRE(in.is_open());

    std::string content;
    std::string line;
    while (std::getline(in, line)) {
        if (!content.empty()) content += '\n';
        content += line;
    }

    CHECK(content.find("line one") != std::string::npos);
    CHECK(content.find("line two") != std::string::npos);

    in.close();
    std::filesystem::remove(path);
}

TEST_CASE("file sink appends across multiple sessions", "[log][sink]") {
    log_fixture fix;

    auto path = std::filesystem::temp_directory_path() / "collab_test_log_append.txt";
    std::filesystem::remove(path);

    add_sink(make_file_sink(path));
    info("first");
    clear_sinks();

    add_sink(make_file_sink(path));
    info("second");
    clear_sinks();

    std::ifstream in(path);
    REQUIRE(in.is_open());

    std::string content;
    std::string line;
    while (std::getline(in, line)) {
        if (!content.empty()) content += '\n';
        content += line;
    }

    CHECK(content.find("first") != std::string::npos);
    CHECK(content.find("second") != std::string::npos);

    in.close();
    std::filesystem::remove(path);
}

TEST_CASE("file sink renders identifier with bundle id", "[log][sink][identifier]") {
    log_fixture fix;

    static const collab::identifier id{
        .app_id   = "lib-x",
        .app_name = "Lib X",
        .org_id   = "purr",
        .org_name = "Purr",
        .tld      = "com",
    };

    auto path = std::filesystem::temp_directory_path() / "collab_test_log_id.txt";
    std::filesystem::remove(path);

    add_sink(make_file_sink(path));
    info_with(id, "tagged");
    clear_sinks();

    std::ifstream in(path);
    REQUIRE(in.is_open());
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    CHECK(content.find("[com.purr.lib-x] tagged") != std::string::npos);

    std::filesystem::remove(path);
}
