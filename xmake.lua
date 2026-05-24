-- ─────────────────────────────────────────────────────────────────────────────
-- Reference config commands (run once per session; -y is mandatory)
--
-- Linux / GCC 15 (release):
--   xmake f -c -y --cc=gcc-15 --cxx=g++-15 --ld=g++-15 --sh=g++-15 -m release
--
-- Linux / Clang + libc++ (release):
--   xmake f -c -y --cc=clang --cxx=clang++ --ld=clang++ --sh=clang++ \
--     --cxxflags="-stdlib=libc++" --ldflags="-stdlib=libc++"
--
-- macOS / brew LLVM (release):
--   xmake f -c -y --toolchain=llvm --sdk="$(brew --prefix llvm)"
-- ─────────────────────────────────────────────────────────────────────────────

add_repositories("BuildWithCollab https://github.com/BuildWithCollab/Packages")

add_rules("mode.release")
set_defaultmode("release")

set_languages("c++23")

option("header_only")
    set_default(true)
    set_showmenu(true)
    set_description("Install collab as a header-only library")
option_end()

add_requires("fmt")

if not get_config("header_only") then
    set_policy("build.c++.modules", true)
    add_requires("spdlog")
    add_requires("rang")
end

if is_plat("windows") then
    add_cxxflags("/utf-8", { public = true })
end

option("build_tests")
    set_default(true)
    set_showmenu(true)
    set_description("Build test targets")
option_end()

target("collab")
if get_config("header_only") then
    set_kind("headeronly")
    add_headerfiles("include/(**.hpp)")
    add_includedirs("include", { public = true })
    add_packages("fmt")
else
    set_kind("static")
    add_files("src/**.cpp")
    add_files("src/**.cppm", { public = true })
    add_includedirs("include", { public = true })
    add_packages("fmt", { public = true })
    add_packages("spdlog")
    add_packages("rang")
end
target_end()

if get_config("build_tests") then
    add_requires("catch2")
    target("tests-collab")
        set_kind("binary")
        add_files("tests/*.cpp") -- Shared tests, for both header-only and static library configurations
        if not get_config("header_only") then
            add_files("tests/static/*.cpp") -- Tests for only the static library configuration
        end
        add_deps("collab")
        add_packages("catch2")
        set_rundir("$(projectdir)")
        add_tests("default", {runargs = {"--durations", "yes"}})
    target_end()
end
