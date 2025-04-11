#pragma once

#include <coroutine>
#include <exception>
#include <optional>

#include "result.hpp"
#include "task_awaiter.hpp"

template <typename ResultType>
struct Task;

template <typename ResultType>
struct TaskPromise {
    using handle_type = std::coroutine_handle<TaskPromise>;
    auto get_return_object() { return Task{handle_type::from_promise(*this)}; }  // NOTE: CTAD

    std::suspend_never initial_suspend() { return {}; }

    std::suspend_always final_suspend() noexcept { return {}; }

    void unhandled_exception() { result_ = Result<ResultType>{std::current_exception()}; }

    void return_value(ResultType value) { result_ = Result{std::move(value)}; }  // NOTE: CTAD

    template <typename _ResultType>
    auto await_transform(Task<_ResultType>&& task) {
        return TaskAwaiter{std::forward(task)};
    }

private:
    std::optional<Result<ResultType>> result_;
};