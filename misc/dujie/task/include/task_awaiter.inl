#pragma once

#include "task_awaiter.hpp"

template <typename ResultType>
TaskAwaiter<ResultType>::TaskAwaiter(TaskAwaiter &&other) noexcept : task_(std::exchange(other.task_, {})) {}

template <typename ResultType>
bool TaskAwaiter<ResultType>::await_ready() {
    return false;
}

template <typename ResultType>
void TaskAwaiter<ResultType>::await_suspend(std::coroutine_handle<> h) noexcept {
    task_.Finally([h]() { h.resume(); });
}

template <typename ResultType>
ResultType TaskAwaiter<ResultType>::await_resume() noexcept {
    return task_.GetResult();
}
