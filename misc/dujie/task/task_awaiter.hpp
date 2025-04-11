#pragma once

#include <coroutine>
#include <utility>

template <typename ResultType>
struct Task;

template <typename Result>
struct TaskAwaiter {
    explicit TaskAwaiter(Task<Result> &&task) noexcept : task_(std::move(task)) {}

    // move-only
    TaskAwaiter(TaskAwaiter &&other) noexcept : task_(std::exchange(other.task_, {})) {}

    TaskAwaiter(TaskAwaiter const &) = delete;

    TaskAwaiter &operator=(TaskAwaiter const &) = delete;

    // 不挂起, 当前协程立即恢复执行
    bool await_ready() { return false; }

    void await_suspend(std::coroutine_handle<> h) noexcept {
        task_.finally([h]() { h.resume(); });
    }

    Result await_resume() noexcept { return task_.get_result(); }

private:
    Task<Result> task_;
};