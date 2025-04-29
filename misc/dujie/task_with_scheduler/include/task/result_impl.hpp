#pragma once

#include <exception>
#include <stdexcept>
#include <variant>

#include "result.hpp"

template <typename ResultType>
Result<ResultType>::Result() : has_value_(false) {}

template <typename ResultType>
Result<ResultType>::Result(ResultType&& value) : value_(std::move(value)), has_value_(true) {}

template <typename ResultType>
Result<ResultType>::Result(std::exception_ptr exception_ptr) : exception_ptr_(exception_ptr), has_value_(false) {}

template <typename ResultType>
bool Result<ResultType>::HasValue() const noexcept {
    return has_value_;
}

template <typename ResultType>
bool Result<ResultType>::HasError() const noexcept {
    return !has_value_ && exception_ptr_;
}

template <typename ResultType>
ResultType& Result<ResultType>::GetOrThrow() {
    if (!has_value_) {
        if (exception_ptr_) {
            std::rethrow_exception(exception_ptr_);
        }
        throw std::logic_error("Result contains no value");
    }
    return value_;
}

template <typename ResultType>
ResultType* Result<ResultType>::ValueOrNull() noexcept {
    return has_value_ ? &value_ : nullptr;
}
