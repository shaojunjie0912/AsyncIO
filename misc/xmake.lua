target("co_await", function()
    set_kind("binary")
    add_files("co_await/main.cpp")
end)

target("co_yield", function()
    set_kind("binary")
    add_files("co_yield/main.cpp")
end)

target("co_return", function()
    set_kind("binary")
    add_files("co_return/main.cpp")
end)

target("scheduling", function()
    set_kind("binary")
    add_files("scheduling/main.cpp")
end)

target("love", function()
    set_kind("binary")
    add_files("love/main.cpp")
end)

target("generator", function()
    set_kind("binary")
    add_files("generator/main.cpp")
end)

target("iota", function()
    set_kind("binary")
    add_files("iota/main.cpp")
end)

target("myfib", function()
    set_kind("binary")
    add_files("state_co/myfib.cpp")
end)
