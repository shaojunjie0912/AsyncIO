#pragma once

#include <coroutine>
#include <utility>

#include "task_forward.hpp"

// TODO: 原来是 Result
template <typename ResultType>
struct TaskAwaiter {
    explicit TaskAwaiter(Task<ResultType> &&task) noexcept;

    // move-only
    TaskAwaiter(TaskAwaiter &&other) noexcept;
    TaskAwaiter(TaskAwaiter const &) = delete;
    TaskAwaiter &operator=(TaskAwaiter const &) = delete;

    // 不挂起, 当前协程立即恢复执行
    bool await_ready();

    void await_suspend(std::coroutine_handle<> h) noexcept;

    // NOTE: 协程恢复执行时, 被等待的 Task 已经执行完, 调用 GetResult 获取结果
    ResultType await_resume() noexcept;

private:
    Task<ResultType> task_;
};

#include "task_awaiter.inl"