#include <asyncio/asyncio.hpp>

using namespace asyncio;

Task<int> factorial(int n) {
    if (n <= 1) {
        co_await DumpCallstack();
        co_return 1;
    }
    co_return (co_await factorial(n - 1)) * n;
}

int main() {
    fmt::print("run result: {}\n", Run(factorial(10)));
    return 0;
}
