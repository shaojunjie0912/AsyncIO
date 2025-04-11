//
// Created by benny on 2022/1/31.
//
#include <coroutine>
#include <exception>
#include <iostream>
#include <thread>
#include <utility>

struct Generator {
    class ExhaustedException : std::exception {};

    struct promise_type {
        int value_;
        bool is_ready_ = false;

        std::suspend_never initial_suspend() { return {}; };

        std::suspend_always final_suspend() noexcept { return {}; }

        std::suspend_always await_transform(int value) {
            this->value_ = value;
            is_ready_ = true;
            return {};
        }

        void unhandled_exception() {}

        Generator get_return_object() { return Generator{std::coroutine_handle<promise_type>::from_promise(*this)}; }

        void return_void() {}
    };

    std::coroutine_handle<promise_type> handle_;

    bool has_next() {
        if (!handle_ || handle_.done()) {
            return false;
        }

        if (!handle_.promise().is_ready_) {
            handle_.resume();
        }

        if (handle_.done()) {
            return false;
        } else {
            return true;
        }
    }

    int next() {
        if (has_next()) {
            handle_.promise().is_ready_ = false;
            return handle_.promise().value_;
        }
        throw ExhaustedException();
    }

    explicit Generator(std::coroutine_handle<promise_type> handle) noexcept : handle_(handle) {}

    Generator(Generator &&generator) noexcept : handle_(std::exchange(generator.handle_, {})) {}

    Generator(Generator &) = delete;
    Generator &operator=(Generator &) = delete;

    ~Generator() {
        if (handle_) handle_.destroy();
    }
};

Generator sequence() {
    int i = 0;
    while (i < 5) {
        co_await i++;
    }
}

// Generator returns_generator() {
//     auto g = sequence();
//     if (g.has_next()) {
//         std::cout << g.next() << std::endl;
//     }
//     return g;
// }

int main() {
    auto generator = sequence();
    // auto generator = returns_generator();
    for (int i = 0; i < 15; ++i) {
        if (generator.has_next()) {
            std::cout << generator.next() << std::endl;
        } else {
            break;
        }
    }

    return 0;
}