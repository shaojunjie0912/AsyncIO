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

-- generator
target("iota", function()
    set_kind("binary")
    add_files("generator/iota.cpp")
end)

target("fibonacci", function()
    set_kind("binary")
    add_files("generator/fibonacci.cpp")
end)

target("simple_sequence", function()
    set_kind("binary")
    add_files("generator/simple_sequence.cpp")
end)


target("myfib", function()
    set_kind("binary")
    add_files("state_co/myfib.cpp")
end)
