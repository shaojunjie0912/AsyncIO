#pragma once

#include <exception>
#include <stdexcept>
#include <variant>

// TODO: variant
// template <typename T>
// using Result = std::variant<T, std::exception_ptr>;

// TODO: Result 类的设计是否很差? 目的是既能返回 ResultType 类型, 又能返回异常类型

template <typename ResultType>
struct Result {
public:
    // 默认构造为空状态或错误状态
    Result() : has_value_(false) {}

    // 值构造函数
    explicit Result(ResultType&& value) : value_(std::move(value)), has_value_(true) {}

    // 异常构造函数
    explicit Result(std::exception_ptr exception_ptr) : exception_ptr_(exception_ptr), has_value_(false) {}

    // 检查是否包含值
    bool HasValue() const noexcept { return has_value_; }

    // 检查是否有错误
    bool HasError() const noexcept { return !has_value_ && exception_ptr_; }

    // 获取值或抛出异常
    ResultType& GetOrThrow() {
        if (!has_value_) {
            if (exception_ptr_) {
                std::rethrow_exception(exception_ptr_);
            }
            throw std::logic_error("Result contains no value");
        }
        return value_;
    }

    // 安全地获取值指针（不抛出异常）
    ResultType* ValueOrNull() noexcept { return has_value_ ? &value_ : nullptr; }

private:
    ResultType value_{};
    std::exception_ptr exception_ptr_;
    bool has_value_ = false;  // 明确的状态标志
};

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
