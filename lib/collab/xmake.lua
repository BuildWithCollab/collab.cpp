target("collab")
    set_kind("static")
    add_files("src/**.cpp")
    add_files("src/**.cppm", { public = true })
    add_packages("collab-hpp", { public = true })
    add_packages("spdlog")
    add_packages("rang")
    -- Use external fmt in spdlog so its bundled fmt doesn't collide with the
    -- standalone fmt reached through collab-hpp's header unit.
    -- Pass as a table to dodge xmake's get_headerunit_key bug (support.lua:154
    -- calls table.concat on `target:get("defines")` and crashes when it's a
    -- bare string instead of a list).
    add_defines({"SPDLOG_FMT_EXTERNAL"})

if get_config("build_tests") then
    target("tests-collab")
        set_kind("binary")
        add_files("tests/*.cpp")
        add_deps("collab")
        add_packages("catch2")
        set_rundir("$(projectdir)")
        add_tests("default", {runargs = {"--durations", "yes"}})
end
