#include <string_view>
//
#include <fmt/core.h>

#include <cutecoro/cutecoro.hpp>

using cutecoro::Task;

Task<std::string_view> Hello() { co_return "hello"; }

Task<std::string_view> World() { co_return "world"; }

Task<std::string> HelloWorld() {
    co_return fmt::format("{} {}", co_await Hello(), co_await World());
}

int main() {
    fmt::print("Run result: {}\n", cutecoro::Run(HelloWorld()));
    return 0;
}
