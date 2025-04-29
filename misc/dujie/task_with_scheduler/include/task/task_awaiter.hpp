#pragma once

#include <coroutine>

#include "task_forward.hpp"
//
#include "executor.hpp"

// NOTE: 万能引用
// - 参数形式必须精确是 T&& (T 是模板参数)
// - T 必须是通过函数模板参数推导得到 (不是类模板参数)

// TODO: 原来是 Result
template <typename ResultType, typename Executor>
struct TaskAwaiter {
    explicit TaskAwaiter(AbstractExecutor *executor, Task<ResultType, Executor> &&task);  // NOTE: 这不是万能引用!!

    // move-only
    TaskAwaiter(TaskAwaiter &&other) noexcept;

    TaskAwaiter(TaskAwaiter const &) = delete;

    TaskAwaiter &operator=(TaskAwaiter const &) = delete;

public:
    // 返回 false 协程即将挂起
    bool await_ready();

    // 协程挂起前的一些操作
    void await_suspend(std::coroutine_handle<> h);

    // NOTE: 协程恢复执行时, 被等待的 Task 已经执行完, 调用 GetResult 获取结果
    ResultType await_resume();

private:
    Task<ResultType, Executor> task_;
    AbstractExecutor *executor_;
};

#include "task_awaiter_impl.hpp"
