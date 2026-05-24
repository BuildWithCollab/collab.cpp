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
set_policy("build.c++.modules", true)

add_requires("fmt")
add_requires("spdlog")
add_requires("rang")
add_requires("catch2")

if is_plat("windows") then
    add_cxxflags("/utf-8", { public = true })
end

target("collab")
    set_kind("static")
    add_files("src/**.cpp")
    add_files("src/**.cppm", { public = true })
    add_includedirs("include", { public = true })
    add_packages("fmt", { public = true })
    add_packages("spdlog")
    add_packages("rang")
target_end()

target("tests-include")
    set_kind("binary")
    add_files("tests/test_include_only.cpp")
    add_deps("collab")
    add_packages("catch2")
    set_rundir("$(projectdir)")
    add_tests("default", {runargs = {"--durations", "yes"}})
target_end()

target("tests-import")
    set_kind("binary")
    add_files("tests/test_import_only.cpp")
    add_deps("collab")
    add_packages("catch2")
    set_rundir("$(projectdir)")
    add_tests("default", {runargs = {"--durations", "yes"}})
target_end()

target("tests-dual")
    set_kind("binary")
    add_files("tests/test_dual.cpp")
    add_deps("collab")
    add_packages("catch2")
    set_rundir("$(projectdir)")
    add_tests("default", {runargs = {"--durations", "yes"}})
target_end()
