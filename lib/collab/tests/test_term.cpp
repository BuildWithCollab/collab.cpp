#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

import collab;

using namespace collab::term;

// ── Helper: stream to a temp file and read back ─────────────────────

static std::string stream_to_string(auto fn) {
    auto path = std::filesystem::temp_directory_path() / "collab_test_term.txt";
    {
        std::ofstream out(path);
        fn(out);
    }
    std::ifstream in(path);
    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    in.close();
    std::filesystem::remove(path);
    return content;
}

// ── Tests ───────────────────────────────────────────────────────────

TEST_CASE("color operator<< writes to ostream", "[term]") {
    auto result = stream_to_string([](std::ostream& os) {
        os << fg::red << "hello" << reset_color;
    });
    CHECK(result.find("hello") != std::string::npos);
}

TEST_CASE("style operator<< writes to ostream", "[term]") {
    auto result = stream_to_string([](std::ostream& os) {
        os << bold << "strong" << reset_style;
    });
    CHECK(result.find("strong") != std::string::npos);
}

TEST_CASE("all fg colors stream through operator<<", "[term]") {
    auto result = stream_to_string([](std::ostream& os) {
        os << fg::black << fg::red << fg::green << fg::yellow
           << fg::blue << fg::magenta << fg::cyan << fg::gray
           << "test" << reset_color;
    });
    CHECK(result.find("test") != std::string::npos);
}

TEST_CASE("all styles stream through operator<<", "[term]") {
    auto result = stream_to_string([](std::ostream& os) {
        os << bold << dim << italic << underline
           << "styled" << reset_style;
    });
    CHECK(result.find("styled") != std::string::npos);
}

TEST_CASE("colors and styles combine in a single stream", "[term]") {
    auto result = stream_to_string([](std::ostream& os) {
        os << bold << fg::yellow << "warning" << reset_style << reset_color;
    });
    CHECK(result.find("warning") != std::string::npos);
}

TEST_CASE("fg convenience constants match enum values", "[term]") {
    CHECK(fg::red == color::red);
    CHECK(fg::green == color::green);
    CHECK(fg::yellow == color::yellow);
    CHECK(fg::blue == color::blue);
    CHECK(fg::magenta == color::magenta);
    CHECK(fg::cyan == color::cyan);
    CHECK(fg::gray == color::gray);
    CHECK(fg::black == color::black);
}

TEST_CASE("top-level convenience constants match enum values", "[term]") {
    CHECK(reset_color == color::reset);
    CHECK(bold == style::bold);
    CHECK(dim == style::dim);
    CHECK(italic == style::italic);
    CHECK(underline == style::underline);
    CHECK(reset_style == style::reset);
}
