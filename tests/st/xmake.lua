target("hello_world", function()
    set_kind("binary")
    add_files("hello_world.cpp")
end)

target("dump_callstack", function()
    set_kind("binary")
    add_files("dump_callstack.cpp")
end)

target("echo_client", function()
    set_kind("binary")
    add_files("echo_client.cpp")
end)

target("echo_server", function()
    set_kind("binary")
    add_files("echo_server.cpp")
end)
