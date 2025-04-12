#pragma once

#include <utility>

#include "task.hpp"
//
#include "task/result.hpp"
#include "task_promise.hpp"

template <typename ResultType>
Task<ResultType>::Task(std::coroutine_handle<promise_type> handle) : h_(handle) {}

template <typename ResultType>
ResultType Task<ResultType>::GetResult() {
    return h_.promise().GetResult();
}

template <typename ResultType>
Task<ResultType> &Task<ResultType>::Then(ResultValueCallback &&func) {
    // 注册当 simple_task 任务完成时(即 result_ 有结果即 co_return 返回时调用 return_value 后)的回调
    h_.promise().OnCompleted([func](Result<ResultType> result) {
        try {
            func(result.GetOrThrow());  // 回调: debug("simple task end: ", i)
        } catch (std::exception const &e) {
        }
    });
    return *this;
}

template <typename ResultType>
Task<ResultType> &Task<ResultType>::Catching(ResultExceptionCallback &&func) {
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
template <typename ResultType>
Task<ResultType> &Task<ResultType>::Finally(std::function<void()> &&func) {
    h_.promise().OnCompleted([func](Result<ResultType>) { func(); });
    return *this;
}

template <typename ResultType>
Task<ResultType>::~Task() noexcept {
    if (h_) {
        h_.destroy();
    }
}

template <typename ResultType>
Task<ResultType>::Task(Task &&other) noexcept : h_(std::exchange(other.h_, {})) {}

template <typename ResultType>
Task<ResultType> &Task<ResultType>::operator=(Task &&other) noexcept {
    if (this != &other) {
        if (h_) {
            h_.destroy();
        }
        h_ = std::exchange(other.h_, {});
    }
    return *this;
}

// void specialization implementations
Task<void>::Task(std::coroutine_handle<promise_type> handle) : h_(handle) {}

void Task<void>::GetResult() { return h_.promise().GetResult(); }

Task<void> &Task<void>::Then(ResultCallback &&func) {
    h_.promise().OnCompleted([func](Result<void> result) {
        try {
            result.GetOrThrow();
            func();
        } catch (std::exception const &) {
        }
    });
    return *this;
}

Task<void> &Task<void>::Catching(ExceptionCallback &&func) {
    h_.promise().OnCompleted([func](Result<void> result) {
        try {
            result.GetOrThrow();
        } catch (std::exception const &e) {
            func(e);
        }
    });
    return *this;
}

Task<void> &Task<void>::Finally(std::function<void()> &&func) {
    h_.promise().OnCompleted([func](Result<void>) { func(); });
    return *this;
}

Task<void>::~Task() noexcept {
    if (h_) {
        h_.destroy();
    }
}

Task<void>::Task(Task &&other) noexcept : h_(std::exchange(other.h_, {})) {}

Task<void> &Task<void>::operator=(Task &&other) noexcept {
    if (this != &other) {
        if (h_) {
            h_.destroy();
        }
        h_ = std::exchange(other.h_, {});
    }
    return *this;
}
