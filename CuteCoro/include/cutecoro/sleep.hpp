#pragma once

#include <chrono>
#include <cutecoro/noncopyable.hpp>
#include <cutecoro/task.hpp>

namespace cutecoro {

namespace detail {

template <typename Duration>
struct SleepAwaiter : private NonCopyable {
    explicit SleepAwaiter(Duration delay) : delay_(delay) {}

    constexpr bool await_ready() noexcept { return false; }

    constexpr void await_resume() const noexcept {}

    template <typename Promise>
    void await_suspend(std::coroutine_handle<Promise> caller) const noexcept {
        GetEventLoop().CallLater(delay_, caller.promise());
    }

private:
    Duration delay_;
};

template <typename Rep, typename Period>
Task<> Sleep(NoWaitAtInitialSuspend, std::chrono::duration<Rep, Period> delay) {
    co_await detail::SleepAwaiter{delay};
}

}  // namespace detail

template <typename Rep, typename Period>
[[nodiscard("忽略 Sleep 的返回值是说不通的!")]]
Task<> Sleep(std::chrono::duration<Rep, Period> delay) {
    return detail::Sleep(no_wait_at_initial_suspend, delay);
}

}  // namespace cutecoro
