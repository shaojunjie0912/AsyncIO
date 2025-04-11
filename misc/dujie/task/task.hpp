#pragma once

#include <coroutine>
#include <functional>

#include "task_promise.hpp"

template <typename ResultType>
struct Task {
    using promise_type = TaskPromise<ResultType>;

    ResultType GetResult() { return h_.promise().get_result(); }

    Task &Then(std::function<void(ResultType)> &&func) {
        h_.promise().on_completed([func](auto result) {
            try {
                func(result.get_or_throw());
            } catch (std::exception &e) {
                // ignore.
            }
        });
        return *this;
    }

    Task &Catching(std::function<void(std::exception &)> &&func) {
        h_.promise().on_completed([func](auto result) {
            try {
                result.get_or_throw();
            } catch (std::exception &e) {
                func(e);
            }
        });
        return *this;
    }

    Task &Finally(std::function<void()> &&func) {
        h_.promise().on_completed([func](auto result) { func(); });
        return *this;
    }

    explicit Task(std::coroutine_handle<promise_type> handle) noexcept : h_(handle) {}

    ~Task() {
        if (h_) h_.destroy();
    }

    // move-only
    Task(Task &&other) noexcept : h_(std::exchange(other.h_, {})) {}
    Task(Task const &) = delete;
    Task &operator=(Task const &) = delete;

private:
    std::coroutine_handle<promise_type> h_;
};