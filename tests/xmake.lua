-- target("demo_epoll_coro", function()
--     set_kind("binary")
--     add_files("demo_epoll_coro.cpp")
-- end)

target("coro_generator1", function()
    set_kind("binary")
    add_files("coro_generator1.cpp")
end)

target("timer_stdin_cb", function()
    set_kind("binary")
    add_files("epoll/timer_stdin/callback.cpp")
end)

target("timer_stdin_co", function()
    set_kind("binary")
    add_files("epoll/timer_stdin/coroutine_epoll.cpp")
end)
