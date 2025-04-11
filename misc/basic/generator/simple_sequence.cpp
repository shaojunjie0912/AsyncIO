#include <coroutine>
#include <iostream>

struct Generator {
    struct promise_type {
        int value_;

        // 开始执行时不挂起，执行到第一个挂起点
        std::suspend_never initial_suspend() { return {}; };

        // 执行结束后不需要挂起
        std::suspend_always final_suspend() noexcept { return {}; }

        // 传值的同时要挂起，值存入 value 当中
        // 先通过 promise.await_transform(expr) 对 expr 转换, 得到 awaitable
        std::suspend_always await_transform(int value) {
            value_ = value;
            return {};
        }

        void unhandled_exception() {}

        auto get_return_object() { return Generator{std::coroutine_handle<promise_type>::from_promise(*this)}; }

        void return_void() {}
    };

    std::coroutine_handle<promise_type> handle_;

    int next() {
        handle_.resume();
        return handle_.promise().value_;
    }
};

Generator Sequence() {
    int i = 0;
    while (true) {
        co_await i++;
    }
}

int main() {
    auto gen = Sequence();
    for (int i = 0; i < 5; ++i) {
        std::cout << gen.next() << std::endl;
    }
}
