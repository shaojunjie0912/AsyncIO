#pragma once

#include <exception>

// TODO: Result 类的设计是否很差?

// Result: 正常返回结果/抛出的异常
template <typename T>
struct Result {
    explicit Result() = default;
    // TODO: 只能右值? 对应 co_await 后的右值
    explicit Result(T&& value) : value_(value) {}

    explicit Result(std::exception_ptr exception_ptr) : exception_ptr_(exception_ptr) {}

    T GetOrThrow() {
        if (exception_ptr_) {
            std::rethrow_exception(exception_ptr_);
        }
        return value_;
    }

private:
    T value_{};
    std::exception_ptr exception_ptr_;
};