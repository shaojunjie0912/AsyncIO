#pragma once

#include <asyncio/exception.hpp>
#include <optional>
#include <variant>

namespace asyncio {

// 结果类封装
template <typename T>
struct Result {
    // 判断 Result 是否有值
    constexpr bool HasValue() const noexcept {
        // nullptr -> 不是 monostate -> 有别的有效值
        return std::get_if<std::monostate>(&result_) == nullptr;
    }

    // 设置 Result 值
    template <typename R>  // NOTE: 加了一个模板参数 R, 这样 R 可转为 T
    constexpr void SetValue(R&& value) noexcept {
        result_.template emplace<T>(std::forward<R>(value));
    }

    // 设置 result_ 异常
    void SetException(std::exception_ptr exception) noexcept { result_ = exception; }

    // 左值对象调用 GetResult() 会拷贝 result_<T> 的值
    constexpr T GetResult() & {
        if (auto exception = std::get_if<std::exception_ptr>(&result_)) {
            std::rethrow_exception(*exception);
        }
        if (auto res = std::get_if<T>(&result_)) {
            return *res;  // 拷贝
        }
        throw NoResultError{};
    }

    // 右值对象调用 GetResult() 会移动 result_<T> 的值
    constexpr T GetResult() && {
        if (auto exception = std::get_if<std::exception_ptr>(&result_)) {
            std::rethrow_exception(*exception);
        }
        if (auto res = std::get_if<T>(&result_)) {
            return std::move(*res);  // 用户用右值对象了, 一定不需要了, 所以直接移动
        }
        throw NoResultError{};
    }

    // ---------------------------------------
    // NOTE: 给 promise_type 继承用
    template <typename R>
    constexpr void return_value(R&& value) noexcept {
        return SetValue(std::forward<R>(value));
    }

    void unhandled_exception() noexcept { result_ = std::current_exception(); }
    // ---------------------------------------

private:
    std::variant<std::monostate, T, std::exception_ptr> result_;  // TODO: variant
};

// 结果类 void 特化
template <>
struct Result<void> {
    constexpr bool HasValue() const noexcept { return result_.has_value(); }

    void GetResult() {
        if (result_.has_value() && *result_ != nullptr) {
            std::rethrow_exception(*result_);
        }
    }

    void SetException(std::exception_ptr exception) noexcept { result_ = exception; }

    // ---------------------------------------
    // NOTE: 给 promise_type 继承用
    void return_void() noexcept {
        result_.emplace(nullptr);  // 给了一个 nullptr, 不是异常
    }

    void unhandled_exception() noexcept { result_ = std::current_exception(); }
    // ---------------------------------------

private:
    std::optional<std::exception_ptr> result_;  // TODO: optional
};

}  // namespace asyncio
