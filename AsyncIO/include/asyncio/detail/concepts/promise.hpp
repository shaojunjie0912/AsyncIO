#pragma once

#include <coroutine>
#include <utility>
//
#include <asyncio/detail/concepts/awaitable.hpp>
#include <asyncio/detail/concepts/future.hpp>

namespace asyncio {

// TODO: Promise

namespace concepts {

template <typename P>
concept Promise = requires(P p) {
    { p.get_return_object() } -> Future;
    { p.initial_suspend() } -> Awaitable;
    { p.final_suspend() } noexcept -> Awaitable;
    p.unhandled_exception();
    requires(requires(int v) { p.return_value(v); } || requires { p.return_void(); });
};

}  // namespace concepts

}  // namespace asyncio

// #include<
