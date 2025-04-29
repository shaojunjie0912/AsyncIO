#pragma once

#include <utility>

#include "task_awaiter.hpp"

// NOTE: 这里的 task_ 由 simple_task2 转来, 而不是 simple_task
template <typename ResultType>
TaskAwaiter<ResultType>::TaskAwaiter(Task<ResultType> &&task) : task_(std::move(task)) {}

template <typename ResultType>
TaskAwaiter<ResultType>::TaskAwaiter(TaskAwaiter &&other) noexcept : task_(std::exchange(other.task_, {})) {}

// 返回 false 挂起当前调用 co_await 的 simple_task 协程
template <typename ResultType>
bool TaskAwaiter<ResultType>::await_ready() {
    return false;
}

// 这里的 h 是 simple_task 协程的句柄, 因为在 simple_task 中调用了 co_await
template <typename ResultType>
void TaskAwaiter<ResultType>::await_suspend(std::coroutine_handle<> h) {
    // NOTE: 在挂起前注册回调函数到 simple_task2 的内部 TaskPromise
    // 此处回调函数功能: 当 simple_task2 任务完成则恢复 simple_task 协程
    task_.Finally([h]() { h.resume(); });
}

template <typename ResultType>
ResultType TaskAwaiter<ResultType>::await_resume() {
    return task_.GetResult();
}
