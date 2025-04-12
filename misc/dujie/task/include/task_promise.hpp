#pragma once

#include <condition_variable>
#include <coroutine>
#include <exception>
#include <functional>
#include <mutex>
#include <optional>
#include <vector>

#include "task_forward.hpp"
//
#include "result.inl"

// TODO: 还是要给 Result 封装一下, 外部不知道是 variant

// TODO:
// TaskPromise 对应的 Task
// co_await 对应的 Task

template <typename ResultType>
struct TaskPromise {
public:
    using ResultCallback = std::function<void(Result<ResultType>)>;
    using handle_type = std::coroutine_handle<TaskPromise>;

    auto get_return_object();
    std::suspend_never initial_suspend();
    std::suspend_always final_suspend() noexcept;
    void unhandled_exception();
    void return_value(ResultType value);

    template <typename _ResultType>
    auto await_transform(Task<_ResultType>&& task);

public:
    ResultType GetResult();
    void OnCompleted(ResultCallback&& func);

private:
    void NotifyCallbacks();

private:
    std::optional<Result<ResultType>> result_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::vector<ResultCallback> callbacks_;
};

// 针对 void 类型的 TaskPromise 类模板特化: 不需要结果的异步任务
template <>
struct TaskPromise<void> {
public:
    using ResultCallback = std::function<void(Result<void>)>;
    using handle_type = std::coroutine_handle<TaskPromise<void>>;

    auto get_return_object();
    std::suspend_never initial_suspend();
    std::suspend_always final_suspend() noexcept;
    void unhandled_exception();
    void return_void();

    template <typename _ResultType>
    auto await_transform(Task<_ResultType>&& task);

    void GetResult();
    void OnCompleted(ResultCallback&& func);

private:
    void NotifyCallbacks();

private:
    std::optional<Result<void>> result_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::vector<ResultCallback> callbacks_;
};

#include "task_promise.inl"
