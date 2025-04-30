#pragma once

// std
#include <chrono>
// cutecoro
#include <cutecoro/event_loop.hpp>
#include <cutecoro/handle.hpp>
#include <cutecoro/noncopyable.hpp>
#include <cutecoro/result.hpp>
#include <cutecoro/scheduled_task.hpp>
#include <cutecoro/task.hpp>

namespace cutecoro {

namespace detail {

template <typename R, typename Duration>
struct WaitForAwaiter : NonCopyable {
    // TODO: concepts::Awaitable
    // 构造函数
    template <concepts::Awaitable Fut>
    WaitForAwaiter(Fut&& fut, Duration timeout)
        : timeout_handle_(*this, timeout),  // 1. 创建检测超时任务
          wait_for_task_{schedule_task(WaitForTask(no_wait_at_initial_suspend,
                                                   std::forward<Fut>(fut)))}  // 2. 创建等待执行任务
    {}

    constexpr bool await_ready() noexcept { return result_.HasValue(); }

    template <typename Promise>
    void await_suspend(std::coroutine_handle<Promise> continuation) noexcept {
        continuation_ = &continuation.promise();
        // set continuation_ to SUSPEND, don't schedule anymore, until it resume continuation_
        continuation_->SetState(Handle::SUSPEND);
    }

    // co_await expr 表达式的结果
    constexpr decltype(auto) await_resume() {
        return std::move(result_).result();  // move 转右值, result() 调用移动
    }

private:
    // 执行实际的等待操作并处理超时逻辑
    template <concepts::Awaitable Fut>
    Task<> WaitForTask(NoWaitAtInitialSuspend, Fut&& fut) {
        try {
            if constexpr (std::is_void_v<R>) {  // void 类型: 仅等待任务完成
                co_await std::forward<Fut>(fut);
            } else {  // 非 void 类型: 等待任务完成并捕获返回值
                result_.SetValue(co_await std::forward<Fut>(fut));
            }
        } catch (...) {
            result_.unhandled_exception();
        }
        EventLoop& loop{GetEventLoop()};
        loop.CancelHandle(timeout_handle_);  // 任务已经完成, 取消处理超时的句柄
        if (continuation_) {
            loop.CallSoon(*continuation_);  // 恢复等待的协程
        }
    }

private:
    Result<R> result_;            // 存储异步操作的结果
    CoroHandle* continuation_{};  // 指向等待此操作的协程句柄

private:
    struct TimeoutHandle : Handle {
        TimeoutHandle(WaitForAwaiter& awaiter, Duration timeout) : awaiter_(awaiter) {
            GetEventLoop().CallLater(timeout, *this);  // 等待 timeout 时间后执行下面的 Run()
        }

        void Run() override final {  // timeout!
            // 由于此操作超时, 因此 1. 取消任务并设置异常为超时错误 2. 立即调度等待此操作的协程
            awaiter_.wait_for_task_.Cancel();
            awaiter_.result_.SetException(std::make_exception_ptr(TimeoutError{}));
            GetEventLoop().CallSoon(*awaiter_.continuation_);
        }

        WaitForAwaiter& awaiter_;
    };

    TimeoutHandle timeout_handle_;         // 处理超时的句柄
    ScheduledTask<Task<>> wait_for_task_;  // 负责等待目标操作的任务
};

// CTAD Guide for WaitForAwaiter
template <concepts::Awaitable Fut, typename Duration>
WaitForAwaiter(Fut&&, Duration)
    -> WaitForAwaiter<AwaitResult<Fut>, Duration>;  // NOTE: AwaitResult<Fut> 就是 R

// TODO: 没看懂为什么要延长以及怎么延长的
// WaitForAwaiterRegistry 存储并管理可等待对象的生命周期
template <concepts::Awaitable Fut, typename Duration>
struct WaitForAwaiterRegistry {
    // 构造函数保存可等待对象和超时时间
    WaitForAwaiterRegistry(Fut&& fut, Duration duration)
        : fut_(std::forward<Fut>(fut)), duration_(duration) {}

    auto operator co_await() && {
        return WaitForAwaiter{std::forward<Fut>(fut_), duration_};  // CTAD
    }

private:
    Fut fut_;            // 延长可等待对象的生命周期
    Duration duration_;  // 超时时间
};

// CTAD Guide for WaitForAwaiterRegistry
template <concepts::Awaitable Fut, typename Duration>
WaitForAwaiterRegistry(Fut&&, Duration) -> WaitForAwaiterRegistry<Fut, Duration>;

template <concepts::Awaitable Fut, typename Rep, typename Period>
Task<AwaitResult<Fut>> WaitFor(NoWaitAtInitialSuspend, Fut&& fut,
                               std::chrono::duration<Rep, Period> timeout) {
    // lift awaitable type(WaitForAwaiterRegistry) to coroutine
    co_return (co_await WaitForAwaiterRegistry{std::forward<Fut>(fut), timeout});  // CTAD
}

}  // namespace detail

// 等待一个异步操作, 但仅等待到指定的时间限制
template <concepts::Awaitable Fut, typename Rep, typename Period>
[[nodiscard("忽略 WaitFor 函数的返回值是说不通的")]]
Task<AwaitResult<Fut>> WaitFor(Fut&& fut, std::chrono::duration<Rep, Period> timeout) {
    return detail::WaitFor(no_wait_at_initial_suspend, std::forward<Fut>(fut), timeout);
}

}  // namespace cutecoro
