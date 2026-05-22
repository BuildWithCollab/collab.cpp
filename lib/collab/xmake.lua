target("collab")
    set_kind("static")
    add_files("src/**.cpp")
    add_files("src/**.cppm", { public = true })
    add_packages("collab-hpp", { public = true })
    add_packages("spdlog")
    add_packages("rang")

if get_config("build_tests") then
    target("tests-collab")
        set_kind("binary")
        add_files("tests/*.cpp")
        add_deps("collab")
        add_packages("catch2")
        set_rundir("$(projectdir)")
        add_tests("default", {runargs = {"--durations", "yes"}})
end
