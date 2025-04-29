#pragma once

#include <exception>
#include <stdexcept>
#include <variant>

#include "task_forward.hpp"

// TODO: variant
// template <typename T>
// using Result = std::variant<T, std::exception_ptr>;

// TODO: Result 类的设计是否很差? 目的是既能返回 ResultType 类型, 又能返回异常类型

template <typename ResultType>
struct Result {
public:
    // 默认构造为空状态或错误状态
    Result();

    // 值构造函数
    explicit Result(ResultType&& value);

    // 异常构造函数
    explicit Result(std::exception_ptr exception_ptr);

    // 检查是否包含值
    bool HasValue() const noexcept;

    // 检查是否有错误
    bool HasError() const noexcept;

    // 获取值或抛出异常
    ResultType& GetOrThrow();

    // 安全地获取值指针（不抛出异常）
    ResultType* ValueOrNull() noexcept;

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
    Result();

    // 异常构造函数
    explicit Result(std::exception_ptr exception_ptr);

    // 检查是否成功
    bool HasValue() const noexcept;

    // 检查是否有错误
    bool HasError() const noexcept;

    // 抛出异常或正常返回
    void GetOrThrow();

private:
    std::exception_ptr exception_ptr_;
    bool has_error_ = false;
};

#include "result_impl.hpp"
