set_project("AsyncIO")
set_xmakever("2.9.8")

set_languages("c++20")

add_rules("mode.debug", "mode.release", "mode.releasedbg")
add_rules("plugin.compile_commands.autoupdate")

set_warnings("allextra")

add_requires("fmt", "catch2")

includes("AsyncIO")
includes("tests")
