#pragma once

#include <coroutine>
#include <cutecoro/detail/noncopyable.hpp>

namespace cutecoro {

namespace detail {

struct CallStackAwaiter {
    constexpr bool await_ready() noexcept { return false; }

    constexpr void await_resume() const noexcept {}

    template <typename Promise>
    bool await_suspend(std::coroutine_handle<Promise> caller) const noexcept {
        caller.promise().DumpBacktrace();
        return false;
    }
};

}  // namespace detail

[[nodiscard]] inline auto DumpCallstack() -> detail::CallStackAwaiter { return {}; }

}  // namespace cutecoro
