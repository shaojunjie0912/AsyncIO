set_project("CuteCoro")
set_xmakever("2.9.8")

set_languages("c++20")

add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate")

-- set_warnings("allextra")
set_defaultmode("debug")
-- 设置工具链 (可选?)

add_requires("fmt", "catch2")

includes("CuteCoro")
includes("tests")
