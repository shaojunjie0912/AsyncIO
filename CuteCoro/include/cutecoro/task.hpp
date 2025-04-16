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

template <typename R = void>
struct PromiseType : CoroHandle, Result<R> {
    using coro_handle = std::coroutine_handle<PromiseType>;
    PromiseType() = default;

public:
    // TODO: 后面理解一下为啥外部调用者要用 no_wait_at_initial_suspend 标记
    template <typename... Args>  // from free function
    PromiseType(NoWaitAtInitialSuspend, Args&&...) : wait_at_initial_suspend_{false} {}

    // 成员函数协程要求 promise_type 的构造函数额外接受一个代表实例的参数
    template <typename Obj, typename... Args>  // from member function
    PromiseType(Obj&&, NoWaitAtInitialSuspend, Args&&...) : wait_at_initial_suspend_{false} {}

public:
    // --------- 协程的 promise 要求 ----------
    auto get_return_object() noexcept { return Task<R>{coro_handle::from_promise(*this)}; }

    auto initial_suspend() noexcept {
        // TODO: 啊? 内部还能再定义 Awaiter?
        // 没有简单的 if wait_at_initial_suspend_ 对应 suspend_always/never
        // 更加灵活定制?
        struct InitialSuspendAwaiter {
            constexpr bool await_ready() const noexcept { return !wait_; }

            constexpr void await_suspend(std::coroutine_handle<>) const noexcept {}

            constexpr void await_resume() const noexcept {}

            const bool wait_{true};
        };
        return InitialSuspendAwaiter{wait_at_initial_suspend_};
    }

    struct FinalAwaiter {
        // final_suspend 返回, 协程完成时始终暂停
        constexpr bool await_ready() const noexcept { return false; }

        template <typename Promise>
        constexpr void await_suspend(std::coroutine_handle<Promise> h) const noexcept {
            // 当前协程执行完毕时, 如果存在等待的协程 cont, 就获取当前事件循环,
            // 并调用其 call_soon 方法, 这个方法会将等待协程的句柄 *cont 添加到事件循环的待执行队列
            // ready_ 中 以便在事件循环的下一次迭代中恢复 (resume) 该等待协程的执行, 通过调用其
            // run() 方法
            if (auto cont = h.promise().continuation_) {
                get_event_loop().call_soon(*cont);
            }
        }

        constexpr void await_resume() const noexcept {}
    };

    auto final_suspend() noexcept { return FinalAwaiter{}; }

public:
    // 重载基类 Handle 的 run() 方法: 恢复协程执行
    void run() override final { coro_handle::from_promise(*this).resume(); }

    // 重载基类 CoroHandle 的 get_frame_info() 方法: 获取帧信息
    std::source_location const& get_frame_info() const override final { return frame_info_; }

    // 重载基类 CoroHandle 的 dump_backtrace() 方法: 打印栈回溯
    void dump_backtrace(size_t depth = 0) const override final {
        fmt::println("[{}] {}", depth, frame_name());  // 打印当前协程
        if (continuation_) {
            continuation_->dump_backtrace(depth + 1);  // 打印包括之前协程的 backtrace
        } else {
            fmt::println("");
        }
    }

public:
    bool const wait_at_initial_suspend_{true};  // 协程体刚开始时是否挂起(默认 true)
    CoroHandle* continuation_{};                // 前一个协程句柄(在等待当前协程完成), 如何构造?
    std::source_location frame_info_{};         // 帧信息 TODO: 给调用?
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

    ~Task() { destory(); }

public:
    // NOTE: decltype 保留引用和 cv 限定符
    // 左值调用 get_result
    decltype(auto) get_result() & { return handle_.promise().result(); }

    // 右值调用 get_result
    decltype(auto) get_result() && { return std::move(handle_.promise()).result(); }

    struct AwaiterBase {
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
            resumer.promise().set_state(Handle::SUSPEND);
            // 设置被 co_await 的协程的 continuation_
            self_coro_.promise().continuation_ = &resumer.promise();
            // 调度被 co_await 的协程
            self_coro_.promise().schedule();
        }

        coro_handle self_coro_{};
    };

    // co_await 运算符重载(左值)
    auto operator co_await() const& noexcept {
        struct Awaiter : AwaiterBase {
            decltype(auto) await_resume() const {
                if (!AwaiterBase::self_coro_) [[unlikely]] {
                    throw InvalidFuture{};
                }
                return AwaiterBase::self_coro_.promise().result();
            }
        };
        return Awaiter{handle_};  // 用被 co_await 的协程的句柄初始化
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
    bool valid() const { return handle_ != nullptr; }

    // 协程是否完成
    bool done() const { return handle_.done(); }

private:
    // 销毁任务(取消调度 + 销毁句柄)
    void destory() {
        if (auto handle{std::exchange(handle_, nullptr)}) {
            handle.promise().cancel();  // TODO: 调用 CoroHandle::cancel
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
