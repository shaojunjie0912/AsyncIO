#pragma once

#include <utility>

namespace co_async {

template <class T = void>
struct NonVoidHelper {
    using Type = T;
};

template <>
struct NonVoidHelper<void> {
    using Type = NonVoidHelper;

    explicit NonVoidHelper() = default;

    // 重载逗号运算符(见底部 example)
    template <class T>
    constexpr friend T &&operator,(T &&t, NonVoidHelper) {
        return std::forward<T>(t);
    }

    char const *repr() const noexcept { return "NonVoidHelper"; }
};

// NOTE: 逗号运算符重载后的使用示例
// template <typename Func>
// auto process(Func func) {
//     // 使用 NonVoidHelper 统一处理返回类型
//     using Result = typename NonVoidHelper<decltype(func())>::Type;

//     // 无论 func() 返回 void 还是其他类型，都能工作
//     Result result = func(), NonVoidHelper<void>{};

//     // 进一步处理 result...
//     return result;
// }

}  // namespace co_async
