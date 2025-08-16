// 解决 void 类型不能实例化

#pragma once

#include <type_traits>

namespace asyncio {

struct VoidValue {};

namespace detail {

// ========== std::type_identity ==========
// template<class T>
// struct type_identity { using type = T; };
// =======================================

// 类型萃取
template <typename T>
struct GetTypeIfVoid : std::type_identity<T> {};

// void 类型特化为 VoidValue
template <>
struct GetTypeIfVoid<void> : std::type_identity<VoidValue> {};

}  // namespace detail

template <typename T>
using GetTypeIfVoid_t = typename detail::GetTypeIfVoid<T>::type;

}  // namespace asyncio