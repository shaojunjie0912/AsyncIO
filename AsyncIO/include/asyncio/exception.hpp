#pragma once

#include <exception>

namespace asyncio {

struct TimeoutError : std::exception {
    [[nodiscard]] char const* what() const noexcept override { return "Timeout!"; }
};

struct NoResultError : std::exception {
    [[nodiscard]] char const* what() const noexcept override { return "Result is unset!"; }
};

struct InvalidFuture : std::exception {
    [[nodiscard]] char const* what() const noexcept override { return "Future is invalid!"; }
};

}  // namespace asyncio
