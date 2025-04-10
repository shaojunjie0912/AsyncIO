-- target("demo_epoll_coro", function()
--     set_kind("binary")
--     add_files("demo_epoll_coro.cpp")
-- end)

target("coro_generator1", function()
    set_kind("binary")
    add_files("coro_generator1.cpp")
end)
