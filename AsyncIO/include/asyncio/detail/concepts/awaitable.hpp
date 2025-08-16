#pragma once

#include <coroutine>
#include <utility>

namespace asyncio {

// TODO: Awaitable

namespace detail {

template <typename A>
struct GetAwaiter : std::type_identity<A> {};

template <typename A>
    requires requires(A&& a) { std::forward<A>(a).operator co_await(); }
struct GetAwaiter<A> : std::type_identity<decltype(std::declval<A>().operator co_await())> {};

template <typename A>
    requires requires(A&& a) {
        operator co_await(std::forward<A>(a));
        requires !(requires { std::forward<A>(a).operator co_await(); });
    }
struct GetAwaiter<A> : std::type_identity<decltype(operator co_await(std::declval<A>()))> {};

template <typename A>
using GetAwaiter_t = typename GetAwaiter<A>::type;

}  // namespace detail

namespace concepts {

template <typename A>
concept Awaitable = requires {
    typename detail::GetAwaiter_t<A>;
    requires requires(detail::GetAwaiter_t<A> awaiter, std::coroutine_handle<> handle) {
        { awaiter.await_ready() } -> std::convertible_to<bool>;
        awaiter.await_suspend(handle);
        awaiter.await_resume();
    };
};

}  // namespace concepts

template <concepts::Awaitable A>
using AwaitResult = decltype(std::declval<detail::GetAwaiter_t<A>>().await_resume());

// check archtypes
static_assert(concepts::Awaitable<std::suspend_always>);
static_assert(concepts::Awaitable<std::suspend_never>);

}  // namespace asyncio
