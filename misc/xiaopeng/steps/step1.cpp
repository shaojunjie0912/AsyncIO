#include <chrono>
#include <coroutine>

#include "debug.hpp"

struct RepeatAwaiter  // awaiter(原始指针) / awaitable(operator->)
{
    bool await_ready() const noexcept { return false; }

    // 返回当前协程句柄则表示协程挂起后执行权返回给自己(其实就是挂起又立即恢复呗)
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> handle) const noexcept {
        if (handle.done()) {  // 协程已经结束, 返回空句柄, 即返回 main
            return std::noop_coroutine();
        } else {
            return handle;
        }
    }

    void await_resume() const noexcept {}
};

struct RepeatAwaitable  // awaitable(operator->)
{
    RepeatAwaiter operator co_await() { return RepeatAwaiter(); }
};

struct Promise {
    auto initial_suspend() { return std::suspend_always(); }

    auto final_suspend() noexcept { return std::suspend_always(); }

    void unhandled_exception() { throw; }

    auto yield_value(int ret) {
        mRetValue = ret;
        return RepeatAwaiter();  // 当前协程挂起, 转移执行权
    }

    void return_void() { mRetValue = 0; }

    std::coroutine_handle<Promise> get_return_object() { return std::coroutine_handle<Promise>::from_promise(*this); }

    int mRetValue;
};

struct Task {
    // 协程返回类型中有 promise_type 类型成员即可, 可以内部 struct, 也可以外部定义内部 using
    using promise_type = Promise;

    // 给协程返回类型写了一个构造函数, 会发生隐式转换
    // 从 Promise 对象中的 get_return_object 函数返回 std::coroutine_handle<promise_type> 隐式转换为 Task
    Task(std::coroutine_handle<promise_type> coroutine) : mCoroutine(coroutine) {}

    std::coroutine_handle<promise_type> mCoroutine;
};

Task hello() {
    debug(), "hello 42";
    co_yield 42;
    debug(), "hello 12";
    co_yield 12;
    debug(), "hello 6";
    co_yield 6;
    debug(), "hello 结束";
    co_return;
}

int main() {
    debug(), "main即将调用hello";
    Task t = hello();
    debug(), "main调用完了hello";
    while (!t.mCoroutine.done()) {
        t.mCoroutine.resume();
        debug(), "main得到hello结果为", t.mCoroutine.promise().mRetValue;
    }
    return 0;
}
