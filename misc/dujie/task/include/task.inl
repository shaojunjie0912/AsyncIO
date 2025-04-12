#pragma once

#include <utility>

#include "task.hpp"
//
#include "task_promise.inl"

template <typename ResultType>
ResultType Task<ResultType>::GetResult() {
    return h_.promise().GetResult();
}

template <typename ResultType>
Task<ResultType> &Task<ResultType>::Then(ResultCallback &&func) {
    h_.promise().OnCompleted([func](auto result) {
        try {
            func(result.GetOrThrow());
        } catch (std::exception const &e) {
        }
    });
    return *this;
}

template <typename ResultType>
Task<ResultType> &Task<ResultType>::Catching(ExceptionCallback &&func) {
    h_.promise().OnCompleted([func](auto result) {
        try {
            result.GetOrThrow();
        } catch (std::exception const &e) {
            func(e);
        }
    });
    return *this;
}

template <typename ResultType>
Task<ResultType> &Task<ResultType>::Finally(std::function<void()> &&func) {
    h_.promise().OnCompleted([func](auto result) { func(); });
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
void Task<void>::GetResult() { return h_.promise().GetResult(); }

Task<void> &Task<void>::Then(ResultCallback &&func) {
    h_.promise().OnCompleted([func](auto result) {
        try {
            result.GetOrThrow();
            func();
        } catch (std::exception const &) {
        }
    });
    return *this;
}

Task<void> &Task<void>::Catching(ExceptionCallback &&func) {
    h_.promise().OnCompleted([func](auto result) {
        try {
            result.GetOrThrow();
        } catch (std::exception const &e) {
            func(e);
        }
    });
    return *this;
}

Task<void> &Task<void>::Finally(std::function<void()> &&func) {
    h_.promise().OnCompleted([func](auto) { func(); });
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
