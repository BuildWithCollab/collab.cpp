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
-- Linux / Clang + libc++ + TSan:
--   xmake f -c -y --cc=clang --cxx=clang++ --ld=clang++ --sh=clang++ \
--     --cxxflags="-stdlib=libc++ -fsanitize=thread -g" \
--     --ldflags="-stdlib=libc++ -fsanitize=thread"
--
-- macOS / brew LLVM (release):
--   xmake f -c -y --toolchain=llvm --sdk="$(brew --prefix llvm)"
--
-- macOS / brew LLVM + TSan:
--   xmake f -c -y --toolchain=llvm --sdk="$(brew --prefix llvm)" \
--     --cxxflags="-stdlib=libc++ -fsanitize=thread -g" \
--     --ldflags="-stdlib=libc++ -fsanitize=thread"
--
-- macOS / brew LLVM + TSan — running:
--   Homebrew's TSan runtime may segfault on newer macOS before the bottle
--   is rebuilt. Work around by loading Apple's runtime at execution time:
--
--   DYLD_LIBRARY_PATH="$(xcode-select -p)/Toolchains/XcodeDefault.xctoolchain/usr/lib/clang/21/lib/darwin" \
--     xmake r tests-collab.core
--
-- TSan note: GCC + libstdc++ + C++20 modules + TSan is currently broken
-- (libstdc++ exposes TU-local TSan helpers from exported templates).
-- Use Clang + libc++ for TSan runs on Linux and macOS.
-- ─────────────────────────────────────────────────────────────────────────────

add_rules("mode.release")
set_defaultmode("release")

set_languages("c++23")
set_policy("build.c++.modules", true)

if is_plat("windows") then
    add_cxxflags("/utf-8", { public = true })
end

-- xmake forces -D_GLIBCXX_USE_CXX11_ABI=0 for GCC < 15 when modules are on
-- (workaround for xmake#2716/#3855). That mismatches the default-new-ABI
-- Catch2 package and breaks the link. Opt back into the new ABI.
set_policy("build.c++.modules.gcc.cxx11abi", true)

add_repositories("BuildWithCollab https://github.com/BuildWithCollab/Packages")

add_requires("collab-hpp")
add_requires("fmt")
add_requires("spdlog")
add_requires("rang")

option("build_tests")
    set_default(true)
    set_showmenu(true)
    set_description("Build test targets")
option_end()

if get_config("build_tests") then
    add_requires("catch2")
end

includes("lib/collab/xmake.lua")
