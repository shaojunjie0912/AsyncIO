#include <string_view>
//
#include <fmt/core.h>

#include <cutecoro/cutecoro.hpp>

using cutecoro::Task;

Task<std::string_view> hello() { co_return "hello"; }

Task<std::string_view> world() { co_return "world"; }

Task<std::string> hello_world() {
    co_return fmt::format("{} {}", co_await hello(), co_await world());
}

int main() {
    fmt::print("run result: {}\n", cutecoro::run(hello_world()));
    return 0;
}
