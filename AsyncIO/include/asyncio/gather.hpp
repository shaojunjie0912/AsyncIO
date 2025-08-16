/**
 *  允许并发执行多个可等待对象 (Awaitable), 并将它们的结果收集到一个元组中.
 *  当所有任务完成或任一任务抛出异常时, 整个gather操作完成.
 */

#pragma once

// std
#include <stdexcept>
#include <tuple>
#include <variant>
// asyncio
#include <asyncio/detail/concepts/awaitable.hpp>
#include <asyncio/detail/noncopyable.hpp>
#include <asyncio/detail/void_value.hpp>
#include <asyncio/task.hpp>

namespace asyncio {

namespace detail {

template <typename... Rs>
class GatherAwaiter : NonCopyable {
    using ResultTypes = std::tuple<GetTypeIfVoid_t<Rs>...>;

public:
    template <concepts::Awaitable... Futs>
    explicit GatherAwaiter(Futs&&... futs)
        : GatherAwaiter(std::make_index_sequence<sizeof...(Futs)>{},
                        std::forward<Futs>(futs)...)  // 委托构造
    {}

    constexpr bool await_ready() noexcept { return IsFinished(); }

    constexpr auto await_resume() const {
        if (auto exception = std::get_if<std::exception_ptr>(&result_)) {
            std::rethrow_exception(*exception);
        }
        if (auto res = std::get_if<ResultTypes>(&result_)) {
            return *res;
        }
        throw std::runtime_error("result is unset");
    }

    template <typename Promise>
    void await_suspend(std::coroutine_handle<Promise> continuation) noexcept {
        continuation_ = &continuation.promise();
        // set continuation_ to SUSPEND, don't schedule anymore, until it resume continuation_
        continuation_->SetState(Handle::SUSPEND);
    }

private:
    template <concepts::Awaitable... Futs, size_t... Is>
    explicit GatherAwaiter(std::index_sequence<Is...>, Futs&&... futs)
        : tasks_{std::make_tuple(
              CollectResult<Is>(no_wait_at_initial_suspend, std::forward<Futs>(futs))...)} {}

    template <size_t Idx, concepts::Awaitable Fut>
    Task<> CollectResult(NoWaitAtInitialSuspend, Fut&& fut) {
        try {
            auto& results = std::get<ResultTypes>(result_);
            if constexpr (std::is_void_v<AwaitResult<Fut>>) {
                co_await std::forward<Fut>(fut);
            } else {
                std::get<Idx>(results) = std::move(co_await std::forward<Fut>(fut));
            }
            ++count_;
        } catch (...) {
            result_ = std::current_exception();
        }
        if (IsFinished()) {
            GetEventLoop().CallSoon(*continuation_);
        }
    }

private:
    bool IsFinished() {
        return (count_ == sizeof...(Rs) || std::get_if<std::exception_ptr>(&result_) != nullptr);
    }

private:
    std::variant<ResultTypes, std::exception_ptr> result_;
    std::tuple<Task<std::void_t<Rs>>...> tasks_;
    CoroHandle* continuation_{};
    int count_{0};
};

// deduction guide
template <concepts::Awaitable... Futs>
GatherAwaiter(Futs&&...) -> GatherAwaiter<AwaitResult<Futs>...>;

template <concepts::Awaitable... Futs>
struct GatherAwaiterRepositry {
    explicit GatherAwaiterRepositry(Futs&&... futs) : futs_(std::forward<Futs>(futs)...) {}

    auto operator co_await() && {
        return std::apply(
            []<concepts::Awaitable... F>(F&&... f) { return GatherAwaiter{std::forward<F>(f)...}; },
            std::move(futs_));
    }

private:
    // futs_ to lift Future's lifetime
    // 1. if Future is rvalue(Fut&&), then move it to tuple(Fut)
    // 2. if Future is xvalue(Fut&&), then move it to tuple(Fut)
    // 3. if Future is lvalue(Fut&), then store as lvalue-ref(Fut&)
    std::tuple<Futs...> futs_;
};

template <concepts::Awaitable... Futs>  // need deduction guide to deduce future type
GatherAwaiterRepositry(Futs&&...) -> GatherAwaiterRepositry<Futs...>;

template <concepts::Awaitable... Futs>
auto Gather(NoWaitAtInitialSuspend,
            Futs&&... futs)  // need NoWaitAtInitialSuspend to lift futures lifetime early
    -> Task<std::tuple<GetTypeIfVoid_t<AwaitResult<Futs>>...>> {  // lift awaitable
                                                                  // type(GatherAwaiterRepositry) to
                                                                  // coroutine
    co_return co_await GatherAwaiterRepositry{std::forward<Futs>(futs)...};
}

}  // namespace detail

template <concepts::Awaitable... Futs>
[[nodiscard("discard gather doesn't make sense")]]
auto Gather(Futs&&... futs) {
    return detail::Gather(no_wait_at_initial_suspend, std::forward<Futs>(futs)...);
}

}  // namespace asyncio