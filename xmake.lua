set_project("CuteCoro")
set_xmakever("2.9.8")

set_languages("c++20")

add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate")

set_warnings("allextra")
set_defaultmode("debug")

add_requires("fmt")
add_packages("fmt")

includes("CuteCoro")
-- includes("tests")
-- includes("misc")
-- includes("task_without_scheduler")
-- includes("task_with_scheduler")
