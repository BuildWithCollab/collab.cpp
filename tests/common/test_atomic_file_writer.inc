// atomic_file_writer tests — fully inline primitive, runs in all three test
// binaries (header-only, import-only, dual). Needs std::filesystem,
// std::fstream, std::thread to be included by the enclosing TU before
// `import collab;` (already arranged by the existing entry points).

#include "../helpers/temp_dir.hpp"

namespace test_afw_detail {

inline std::string read_file(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    REQUIRE(in.good());
    std::stringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

inline std::size_t count_temp_residue(const std::filesystem::path& dir,
                                      const std::filesystem::path& target_stem) {
    std::size_t count = 0;
    for (const auto& entry : std::filesystem::directory_iterator{dir}) {
        const auto name = entry.path().filename().string();
        if (name.rfind(target_stem.filename().string() + ".", 0) == 0 &&
            name.size() > 4 &&
            name.substr(name.size() - 4) == ".tmp")
        {
            ++count;
        }
    }
    return count;
}

}  // namespace test_afw_detail

TEST_CASE("atomic_file_writer: free function writes new file", "[atomic_file_writer]") {
    collab::test_helpers::temp_dir td{"afw-newfile"};
    const auto target = td.path / "config.toml";

    collab::atomic_file_write(target, std::string_view{"hello"});

    REQUIRE(std::filesystem::exists(target));
    REQUIRE(test_afw_detail::read_file(target) == "hello");
    REQUIRE(test_afw_detail::count_temp_residue(td.path, target) == 0);
}

TEST_CASE("atomic_file_writer: free function overwrites existing file", "[atomic_file_writer]") {
    collab::test_helpers::temp_dir td{"afw-overwrite"};
    const auto target = td.path / "config.toml";

    collab::atomic_file_write(target, std::string_view{"v1"});
    collab::atomic_file_write(target, std::string_view{"v2-longer"});

    REQUIRE(test_afw_detail::read_file(target) == "v2-longer");
    REQUIRE(test_afw_detail::count_temp_residue(td.path, target) == 0);
}

TEST_CASE("atomic_file_writer: class incremental writes concatenate", "[atomic_file_writer]") {
    collab::test_helpers::temp_dir td{"afw-incremental"};
    const auto target = td.path / "config.toml";

    {
        collab::atomic_file_writer w{target};
        w.write(std::string_view{"part1-"});
        w.write(std::string_view{"part2-"});
        w.write(std::string_view{"part3"});
        w.commit();
    }

    REQUIRE(test_afw_detail::read_file(target) == "part1-part2-part3");
    REQUIRE(test_afw_detail::count_temp_residue(td.path, target) == 0);
}

TEST_CASE("atomic_file_writer: free function and class produce identical files", "[atomic_file_writer]") {
    collab::test_helpers::temp_dir td{"afw-equiv"};
    const auto a = td.path / "via-free.toml";
    const auto b = td.path / "via-class.toml";

    const std::string payload = "the quick brown fox jumps over the lazy dog";
    collab::atomic_file_write(a, std::string_view{payload});

    {
        collab::atomic_file_writer w{b};
        w.write(std::string_view{payload});
        w.commit();
    }

    REQUIRE(test_afw_detail::read_file(a) == payload);
    REQUIRE(test_afw_detail::read_file(b) == payload);
}

TEST_CASE("atomic_file_writer: dtor without commit discards temp and leaves target untouched", "[atomic_file_writer]") {
    collab::test_helpers::temp_dir td{"afw-discard"};
    const auto target = td.path / "config.toml";

    collab::atomic_file_write(target, std::string_view{"original"});

    {
        collab::atomic_file_writer w{target};
        w.write(std::string_view{"garbage that never commits"});
        // no commit — dtor must discard
    }

    REQUIRE(test_afw_detail::read_file(target) == "original");
    REQUIRE(test_afw_detail::count_temp_residue(td.path, target) == 0);
}

TEST_CASE("atomic_file_writer: exception between write and commit leaves target untouched", "[atomic_file_writer]") {
    collab::test_helpers::temp_dir td{"afw-throw"};
    const auto target = td.path / "config.toml";

    collab::atomic_file_write(target, std::string_view{"original"});

    try {
        collab::atomic_file_writer w{target};
        w.write(std::string_view{"in-progress"});
        throw std::runtime_error{"simulated failure"};
    } catch (const std::runtime_error&) {
        // expected
    }

    REQUIRE(test_afw_detail::read_file(target) == "original");
    REQUIRE(test_afw_detail::count_temp_residue(td.path, target) == 0);
}

TEST_CASE("atomic_file_writer: writing into a missing directory throws", "[atomic_file_writer]") {
    collab::test_helpers::temp_dir td{"afw-missingdir"};
    const auto target = td.path / "does" / "not" / "exist" / "config.toml";

    REQUIRE_THROWS_AS(
        collab::atomic_file_write(target, std::string_view{"x"}),
        collab::errors::atomic_file_write::error);
}

TEST_CASE("atomic_file_writer: exception is the right leaf type and carries target + os_error", "[atomic_file_writer]") {
    collab::test_helpers::temp_dir td{"afw-payload"};
    const auto target = td.path / "does-not-exist-dir" / "config.toml";

    try {
        collab::atomic_file_write(target, std::string_view{"x"});
        FAIL("expected throw");
    } catch (const collab::errors::atomic_file_write::create_temp_failed& e) {
        REQUIRE(e.target == target);
        REQUIRE(e.os_error != 0);
    }
}

TEST_CASE("atomic_file_writer: family base catches any leaf", "[atomic_file_writer]") {
    collab::test_helpers::temp_dir td{"afw-familycatch"};
    const auto target = td.path / "nope" / "config.toml";

    try {
        collab::atomic_file_write(target, std::string_view{"x"});
        FAIL("expected throw");
    } catch (const collab::errors::atomic_file_write::error& e) {
        REQUIRE(e.target == target);
        REQUIRE(e.os_error != 0);
    }
}

TEST_CASE("atomic_file_writer: collab::error catches the family", "[atomic_file_writer]") {
    collab::test_helpers::temp_dir td{"afw-collabcatch"};
    const auto target = td.path / "nope" / "config.toml";

    try {
        collab::atomic_file_write(target, std::string_view{"x"});
        FAIL("expected throw");
    } catch (const collab::error&) {
        SUCCEED();
    }
}

TEST_CASE("atomic_file_writer: zero-byte write produces zero-byte file", "[atomic_file_writer]") {
    collab::test_helpers::temp_dir td{"afw-empty"};
    const auto target = td.path / "config.toml";

    collab::atomic_file_write(target, std::string_view{""});

    REQUIRE(std::filesystem::exists(target));
    REQUIRE(std::filesystem::file_size(target) == 0);
}

TEST_CASE("atomic_file_writer: large multi-chunk payload survives round-trip", "[atomic_file_writer]") {
    collab::test_helpers::temp_dir td{"afw-large"};
    const auto target = td.path / "config.toml";

    std::string payload;
    payload.resize(1024 * 1024);  // 1 MB
    for (std::size_t i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<char>('A' + (i % 26));
    }

    {
        collab::atomic_file_writer w{target};
        w.write(std::string_view{payload});
        w.commit();
    }

    REQUIRE(test_afw_detail::read_file(target) == payload);
}

TEST_CASE("atomic_file_writer: target() reflects the constructor path", "[atomic_file_writer]") {
    collab::test_helpers::temp_dir td{"afw-target"};
    const auto target = td.path / "config.toml";
    collab::atomic_file_writer w{target};
    REQUIRE(w.target() == target);
}

TEST_CASE("atomic_file_writer: direct_write_fallback getter/setter", "[atomic_file_writer]") {
    collab::test_helpers::temp_dir td{"afw-fallback"};
    const auto target = td.path / "config.toml";
    collab::atomic_file_writer w{target};
    REQUIRE_FALSE(w.direct_write_fallback());
    w.set_direct_write_fallback(true);
    REQUIRE(w.direct_write_fallback());
}
