#pragma once

#include <exception>
#include <stdexcept>
#include <variant>

#include "result.hpp"

template <typename ResultType>
Result<ResultType>::Result() : has_value_(false) {}

// explicit Result(ResultType&& value) : value_(std::move(value)), has_value_(true) {}

// explicit Result(std::exception_ptr exception_ptr) : exception_ptr_(exception_ptr), has_value_(false) {}

// bool HasValue() const noexcept { return has_value_; }

// bool HasError() const noexcept { return !has_value_ && exception_ptr_; }

// ResultType& GetOrThrow() {
//     if (!has_value_) {
//         if (exception_ptr_) {
//             std::rethrow_exception(exception_ptr_);
//         }
//         throw std::logic_error("Result contains no value");
//     }
//     return value_;
// }

// ResultType* ValueOrNull() noexcept { return has_value_ ? &value_ : nullptr; }

// 针对 void 类型的 Result 类模板特化: 不需要结果的异步任务
template <>
struct Result<void> {
public:
    // 默认构造为成功状态
    Result() : has_error_(false) {}

    // 异常构造函数
    explicit Result(std::exception_ptr exception_ptr) : exception_ptr_(exception_ptr), has_error_(true) {}

    // 检查是否成功
    bool HasValue() const noexcept { return !has_error_; }

    // 检查是否有错误
    bool HasError() const noexcept { return has_error_; }

    // 抛出异常或正常返回
    void GetOrThrow() {
        if (has_error_ && exception_ptr_) {
            std::rethrow_exception(exception_ptr_);
        }
    }

private:
    std::exception_ptr exception_ptr_;
    bool has_error_ = false;
};
