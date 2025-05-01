#pragma once

#include <fmt/format.h>

#include <cassert>
#include <coroutine>
#include <source_location>
#include <utility>
//
#include <cutecoro/concepts/promise.hpp>
#include <cutecoro/event_loop.hpp>
#include <cutecoro/handle.hpp>
#include <cutecoro/result.hpp>

namespace cutecoro {

struct NoWaitAtInitialSuspend {};

inline constexpr NoWaitAtInitialSuspend no_wait_at_initial_suspend;

template <typename R>
struct Task;

// PromiseType 继承自 CoroHandle 和 Result
// - 具有 CoroHandle 的一些方法 (Schedule)
// - 具有 Result 的一些方法 (return_value/void)
template <typename R = void>
struct PromiseType : CoroHandle, Result<R> {
    using coro_handle = std::coroutine_handle<PromiseType>;

public:
    // 构造函数
    // NOTE: 捕获 source location
    PromiseType(std::source_location loc = std::source_location::current()) : frame_info_(loc) {}

    // 构造函数 (参数: 不挂起)
    template <typename... Args>  // from free function
    PromiseType(NoWaitAtInitialSuspend, Args&&...) : wait_at_initial_suspend_{false} {}

    // 成员函数协程要求 promise_type 的构造函数额外接受一个代表实例的参数
    template <typename Obj, typename... Args>  // from member function
    PromiseType(Obj&&, NoWaitAtInitialSuspend, Args&&...) : wait_at_initial_suspend_{false} {}

public:
    // --------- 协程 promise_type 要求 ----------
    auto get_return_object() noexcept { return Task<R>{coro_handle::from_promise(*this)}; }

    // 协程刚开始执行时是否挂起
    auto initial_suspend() noexcept {
        // auto 无法同时推断 std::suspend_always/never, 因此这里自定义 Awaiter
        struct InitialSuspendAwaiter {
            constexpr bool await_ready() const noexcept { return !wait_; }

            constexpr void await_suspend(std::coroutine_handle<>) const noexcept {}

            constexpr void await_resume() const noexcept {}

            bool wait_;  // 是否挂起 (由外部 wait_at_initial_suspend_ 初始化)
        };
        return InitialSuspendAwaiter{wait_at_initial_suspend_};
    }

    struct FinalAwaiter {
        // final_suspend 返回, 协程完成时始终挂起
        constexpr bool await_ready() const noexcept { return false; }

        // 挂起前做以下操作
        // NOTE: 所以 Hello() 能返回到 HelloWorld()
        template <typename Promise>
        constexpr void await_suspend(std::coroutine_handle<Promise> h) const noexcept {
            // 当前协程执行完毕, 如果存在等待的协程 cont, 则获取事件循环并调用 CallSoon()
            // 将等待协程的句柄 *cont 添加到事件循环的待执行队列 ready_ 中
            // 以便在事件循环的下一次迭代中通过调用句柄的 Run() 恢复 resume 该等待协程的执行
            if (auto cont = h.promise().continuation_) {
                GetEventLoop().CallSoon(*cont);
            }
        }

        constexpr void await_resume() const noexcept {}
    };

    auto final_suspend() noexcept { return FinalAwaiter{}; }

public:
    // 重载基类 Handle 的 Run() 方法: 恢复协程执行
    void Run() override final { coro_handle::from_promise(*this).resume(); }

    // 重载基类 CoroHandle 的 GetFrameInfo() 方法: 获取帧信息
    std::source_location const& GetFrameInfo() const override final { return frame_info_; }

    // 重载基类 CoroHandle 的 DumpBacktrace() 方法: 打印栈回溯
    void DumpBacktrace(size_t depth = 0) const override final {
        fmt::println("[{}] {}", depth, FrameName());  // 打印当前协程
        if (continuation_) {
            continuation_->DumpBacktrace(depth + 1);  // 打印包括之前协程的 backtrace
        } else {
            fmt::println("");
        }
    }

public:
    bool const wait_at_initial_suspend_{true};  // 协程体刚开始时是否挂起(默认 true)
    CoroHandle* continuation_{};       // TODO: 前一个协程句柄(在等待当前协程完成), 如何构造?
    std::source_location frame_info_;  // 帧信息 TODO: 给调用?
};

template <typename R = void>
struct Task : NonCopyable {
    using promise_type = PromiseType<R>;
    using coro_handle = std::coroutine_handle<promise_type>;

    // 友元类
    template <concepts::Future>
    friend struct ScheduledTask;

public:
    explicit Task(coro_handle h) noexcept : handle_(h) {}

    Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}

    ~Task() { Destroy(); }

public:
    // NOTE: decltype 保留引用和 cv 限定符
    // 左值调用 GetResult
    decltype(auto) GetResult() & { return handle_.promise().result(); }

    // 右值调用 GetResult
    decltype(auto) GetResult() && { return std::move(handle_.promise()).result(); }

    // AwaiterBase 基类
    struct AwaiterBase {
        coro_handle self_coro_{};  // 被 co_await 的协程句柄

        constexpr bool await_ready() {
            if (self_coro_) [[likely]] {
                return self_coro_.done();  // 被 co_await 的协程是否已经完成
            }
            return true;
        }

        template <typename Promise>
        void await_suspend(std::coroutine_handle<Promise> resumer) const noexcept {
            // 被 co_await 的协程还没有设置 continuation_
            assert(!self_coro_.promise().continuation_);
            // 标志 continuation_ 协程即调用 co_await 的协程将挂起
            resumer.promise().SetState(Handle::SUSPEND);
            // 设置被 co_await 的协程的 continuation_
            self_coro_.promise().continuation_ = &resumer.promise();
            // 将被 co_await 的协程加入调度(立即执行)
            self_coro_.promise().Schedule();
        }
    };

    // co_await 运算符重载(左值)
    auto operator co_await() const& noexcept {
        struct Awaiter : AwaiterBase {
            decltype(auto) await_resume() const {
                if (!AwaiterBase::self_coro_) [[unlikely]] {
                    throw InvalidFuture{};
                }
                // 被 co_await 的协程执行结束后, 返回自己的结果
                return AwaiterBase::self_coro_.promise().result();
            }
        };
        return Awaiter{handle_};  // HACK: 用被 co_await 的协程的句柄初始化
    }

    // co_await 运算符重载(右值)
    auto operator co_await() const&& noexcept {
        struct Awaiter : AwaiterBase {
            decltype(auto) await_resume() const {
                if (!AwaiterBase::self_coro_) [[unlikely]] {
                    throw InvalidFuture{};
                }
                return std::move(AwaiterBase::self_coro_.promise()).result();
            }
        };
        return Awaiter{handle_};
    }

public:
    // 协程句柄是否有效
    bool IsValid() const { return handle_ != nullptr; }

    // 协程是否完成
    bool IsDone() const { return handle_.done(); }

private:
    // 销毁任务(取消调度 + 销毁句柄)
    void Destroy() {
        if (auto handle{std::exchange(handle_, nullptr)}) {
            handle.promise().Cancel();  // TODO: 调用 CoroHandle::Cancel
            handle.destroy();
        }
    }

private:
    coro_handle handle_;  // 当前协程句柄
};

// TODO: 安全
static_assert(concepts::Promise<Task<>::promise_type>);
static_assert(concepts::Future<Task<>>);

}  // namespace cutecoro
