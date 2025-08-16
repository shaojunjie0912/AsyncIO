#include <string_view>
//
#include <fmt/core.h>

#include <chrono>
#include <asyncio/asyncio.hpp>
#include <thread>

using asyncio::Task;

using namespace std::chrono_literals;

Task<std::string_view> Hello() {
    fmt::println("Hello: sleep 1s");
    std::this_thread::sleep_for(1s);
    fmt::println("Hello: sleep 2s");
    std::this_thread::sleep_for(2s);
    co_return "hello";
}

Task<std::string_view> World() {
    fmt::println("World: sleep 1s");
    std::this_thread::sleep_for(1s);
    fmt::println("World: sleep 2s");
    std::this_thread::sleep_for(2s);
    co_return "world";
}

// co_await 将控制权交给事件循环? 事件循环调度 Hello() 任务执行
Task<std::string> HelloWorld() {
    co_return fmt::format("{} {}", co_await Hello(), co_await World());
}

int main() {
    // NOTE: 协程创建时默认挂起
    // 被 co_await 的协程在 await_suspend 中调用了 Schedule
    // 而这里的 HelloWorld 是在 Run 中的 ScheduledTask 的构造函数中被 Schedule 的
    fmt::print("Run result: {}\n", asyncio::Run(HelloWorld()));
    return 0;
}
