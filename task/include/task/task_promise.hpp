#pragma once

#include <condition_variable>
#include <coroutine>
#include <exception>
#include <functional>
#include <mutex>
#include <optional>
#include <vector>

#include "task/task_awaiter.hpp"
#include "task_forward.hpp"
//
#include "result.hpp"

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

    // TODO: 类模板 TaskPormise 中的函数模板 await_transform 的模板参数
    // 要支持跟类模板 Task 不同的类型, 允许一个协程内部可以 co_await 不同类型的 Task
    template <typename OtherResultType>
    TaskAwaiter<OtherResultType> await_transform(Task<OtherResultType>&& task);

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

    template <typename OtherResultType>
    TaskAwaiter<OtherResultType> await_transform(Task<OtherResultType>&& task);

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
