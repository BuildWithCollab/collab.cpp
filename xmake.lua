add_rules("mode.release")
set_defaultmode("release")

set_languages("c++23")
set_policy("build.c++.modules", true)
set_policy("build.c++.modules.gcc.cxx11abi", true)

option("header_only")
    set_default(false)
    set_showmenu(true)
    set_description("Install header-only library")
option_end()

option("build_tests")
    set_default(true)
    set_showmenu(true)
    set_description("Build test targets")
option_end()

-- Some toolchains (notably MSVC VS2022) do not surface partial specializations
-- of templates from other namespaces (std::hash, std::formatter, fmt::formatter
-- on a fixed_string) from the module's GMF to importer translation units. On
-- those toolchains the pure-import-only tests that exercise these specializations
-- fail to compile. Set this to false for VS2022 CI; users on VS2022 who want
-- those specializations must `#include <collab.hpp>` alongside `import collab;`.
option("test_pure_import_specializations")
    set_default(true)
    set_showmenu(true)
    set_description("Include tests that exercise foreign-template specializations through the pure-import-only path (default true; set false on MSVC VS2022).")
option_end()

if not get_config("test_pure_import_specializations") then
    add_defines("COLLAB_SKIP_PURE_IMPORT_SPECIALIZATION_TESTS")
end

-- Some toolchains (notably GCC 14) do not surface namespace-scope deduction
-- guides through the module's GMF to importer translation units. On those
-- toolchains the pure-import-only test binary fails to build at all because
-- CTAD on basic_fixed_string can't see its deduction guides. Set this to
-- false for GCC 14 CI; users on GCC 14 who want pure-import must `#include
-- <collab.hpp>` alongside `import collab;`. The tests-include and tests-dual
-- binaries are unaffected by this option.
option("build_pure_import_tests")
    set_default(true)
    set_showmenu(true)
    set_description("Build the tests-import binary (pure `import collab;` consumer). Default true; set false on GCC 14.")
option_end()

if get_config("header_only") then
    add_requires("fmt", { configs = { header_only = true } })
else
    add_requires("fmt")
    add_requires("spdlog")
    add_requires("rang")
end

if get_config("build_tests") then
    add_requires("catch2")
end

if is_plat("windows") then
    add_cxxflags("/utf-8", { public = true })
end

-- ─── Static library ─────────────────────────────────────────────────────────
-- Aggregates the canonical inline headers, the generated module partitions
-- (src/<area>.cppm), the generated force-emission impl units
-- (src/<area>_impl.cpp), the umbrella src/collab.cppm, and the hand-written
-- native impl files (src/<area>_native.cpp).
if get_config("header_only") then
    target("collab")
        set_kind("headeronly")
        add_includedirs("include", { public = true })
        add_headerfiles("include/(**.hpp)")
        add_packages("fmt", { public = true })
    target_end()
else
    target("collab")
        set_kind("static")
        add_files("src/**.cpp")
        add_files("src/**.cppm", { public = true })
        add_includedirs("include", { public = true })
        add_headerfiles("include/(**.hpp)")
        add_packages("fmt", { public = true })
        add_packages("spdlog")
        add_packages("rang")
    target_end()
end

if get_config("build_tests") then
    -- ─── Tests ──────────────────────────────────────────────────────────────────
    -- tests-include: pure header-only — does NOT link `collab`. Exercises only
    -- the surface available through <collab.hpp> with no external impl. fmt is
    -- still linked (it's a transitive header dep, and we don't want FMT_HEADER_ONLY
    -- in tests).
    target("tests-include")
        set_kind("binary")
        add_files("tests/test_include_only.cpp")
        add_includedirs("include")
        add_packages("fmt")
        add_packages("catch2")
        set_rundir("$(projectdir)")
        add_tests("default", {runargs = {"--durations", "yes"}})
    target_end()

    if not get_config("header_only") then
        if get_config("build_pure_import_tests") then
            -- tests-import: `import collab;` only, links the static library.
            target("tests-import")
                set_kind("binary")
                add_files("tests/test_import_only.cpp")
                add_deps("collab")
                add_packages("catch2")
                set_rundir("$(projectdir)")
                add_tests("default", {runargs = {"--durations", "yes"}})
            target_end()
        end

        -- tests-dual: both `#include <collab.hpp>` AND `import collab;` in the same TU.
        -- The architecture's load-bearing test.
        target("tests-dual")
            set_kind("binary")
            add_files("tests/test_dual.cpp")
            add_deps("collab")
            add_packages("catch2")
            set_rundir("$(projectdir)")
            add_tests("default", {runargs = {"--durations", "yes"}})
        target_end()
    end
end

-- ─── Codegen ────────────────────────────────────────────────────────────────
-- Regenerates the per-area decls header, module partition, and force-emission
-- impl unit from each canonical inline header in include/collab/*.hpp.
-- Run with: xmake codegen
task("codegen")
    on_run(function ()
        os.exec("python scripts/generate.py --name collab --include include/collab --src src")
    end)
    set_menu({
        usage       = "xmake codegen",
        description = "Regenerate per-area decls headers, module partitions, and impl units."
    })
