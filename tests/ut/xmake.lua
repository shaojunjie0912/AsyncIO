target("test_counted", function()
    set_kind("binary")
    add_files("test_counted.cpp")
end)

target("test_selector", function()
    set_kind("binary")
    add_files("test_selector.cpp")
end)

target("test_result", function()
    set_kind("binary")
    add_files("test_result.cpp")
end)

target("test_task", function()
    set_kind("binary")
    add_files("test_task.cpp")
end)

