// 实现了一种资源清理机制，类似于Python的try-finally语句或Java的try-with-resources，
// 使开发者能够确保无论函数如何退出(正常返回、异常抛出等)，特定代码总会被执行。

#pragma once

#include <utility>

namespace cutecoro {

template <class F>
class FinalAction {
public:
    FinalAction(F f) noexcept : f_(std::move(f)), invoke_(true) {}

    ~FinalAction() noexcept {
        if (invoke_) {
            f_();
        }
    }

    FinalAction(FinalAction &&other) noexcept
        : f_(std::move(other.f_)), invoke_(std::exchange(other.invoke_, false)) {}

    FinalAction(FinalAction const &) = delete;

    FinalAction &operator=(FinalAction const &) = delete;

private:
    F f_;          // 存储的函数对象
    bool invoke_;  // 是否执行函数的标志
};

// 重载为了 lambda 正确推导 const
template <class F>
FinalAction<F> _finally(const F &f) noexcept {
    return FinalAction<F>(f);
}

template <class F>
FinalAction<F> _finally(F &&f) noexcept {
    return FinalAction<F>(std::forward<F>(f));
}

#define concat1(a, b) a##b                                      // 字符串拼接
#define concat2(a, b) concat1(a, b)                             // 中转宏, 确保被展开
#define _finally_object concat2(_finally_object_, __COUNTER__)  // 自动生成唯一变量名
#define finally cutecoro::FinalAction _finally_object = [&]()   // lambda 作为清理函数
#define finally2(func) \
    cutecoro::FinalAction _finally_object = cutecoro::_finally(func)  // 已有的清理函数

}  // namespace cutecoro
