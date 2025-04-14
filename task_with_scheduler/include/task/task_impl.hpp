#pragma once

#include <utility>

#include "task.hpp"
//
#include "result.hpp"
#include "task_promise.hpp"

template <typename ResultType, typename Executor>
Task<ResultType, Executor>::Task(std::coroutine_handle<promise_type> handle) : h_(handle) {}

template <typename ResultType, typename Executor>
ResultType Task<ResultType, Executor>::GetResult() {
    return h_.promise().GetResult();
}

template <typename ResultType, typename Executor>
Task<ResultType, Executor> &Task<ResultType, Executor>::Then(ResultValueCallback &&func) {
    // 注册当 simple_task 任务完成时(即 result_ 有结果即 co_return 返回时调用 return_value 后)的回调
    h_.promise().OnCompleted([func](Result<ResultType> result) {
        try {
            func(result.GetOrThrow());  // 回调: debug("simple task end: ", i)
        } catch (std::exception const &e) {
        }
    });
    return *this;
}

template <typename ResultType, typename Executor>
Task<ResultType, Executor> &Task<ResultType, Executor>::Catching(ResultExceptionCallback &&func) {
    h_.promise().OnCompleted([func](Result<ResultType> result) {
        try {
            result.GetOrThrow();  // 获取结果中可能存在的异常, 如果没有异常, 自然没作用
        } catch (std::exception const &e) {
            func(e);  // 如果结果中有异常则处理异常
        }
    });
    return *this;
}

// NOTE: [h]() { h.resume(); } 传到这里 func 来了
template <typename ResultType, typename Executor>
Task<ResultType, Executor> &Task<ResultType, Executor>::Finally(std::function<void()> &&func) {
    h_.promise().OnCompleted([func](Result<ResultType>) { func(); });
    return *this;
}

template <typename ResultType, typename Executor>
Task<ResultType, Executor>::~Task() noexcept {
    if (h_) {
        h_.destroy();
    }
}

template <typename ResultType, typename Executor>
Task<ResultType, Executor>::Task(Task &&other) noexcept : h_(std::exchange(other.h_, {})) {}

template <typename ResultType, typename Executor>
Task<ResultType, Executor> &Task<ResultType, Executor>::operator=(Task &&other) noexcept {
    if (this != &other) {
        if (h_) {
            h_.destroy();
        }
        h_ = std::exchange(other.h_, {});
    }
    return *this;
}
