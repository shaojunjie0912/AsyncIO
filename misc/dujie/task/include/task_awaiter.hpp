#pragma once

#include <coroutine>
#include <utility>

template <typename ResultType>
struct Task;

// TODO: 原来是 Result
template <typename ResultType>
struct TaskAwaiter {
    explicit TaskAwaiter(Task<ResultType> &&task) noexcept : task_(std::move(task)) {}

    // move-only
    TaskAwaiter(TaskAwaiter &&other) noexcept : task_(std::exchange(other.task_, {})) {}
    // TaskAwaiter &operator=(TaskAwaiter &&other) noexcept {
    //     // TODO: Task 类资源如何释放?
    // }
    TaskAwaiter(TaskAwaiter const &) = delete;
    TaskAwaiter &operator=(TaskAwaiter const &) = delete;

    // 不挂起, 当前协程立即恢复执行
    bool await_ready() { return false; }

    void await_suspend(std::coroutine_handle<> h) noexcept {
        // Task 执行完后调用 resume
        task_.Finally([h]() { h.resume(); });
    }

    // NOTE: 协程恢复执行时, 被等待的 Task 已经执行完, 调用 GetResult 获取结果
    ResultType await_resume() noexcept { return task_.GetResult(); }

private:
    Task<ResultType> task_;
};