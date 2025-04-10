#include <coroutine>

#include "debug.hpp"

struct PreviousAwaiter {
    std::coroutine_handle<> mPrevious;

    bool await_ready() const noexcept { return false; }

    // 返回关联的 mPrevious 协程句柄, 用于协程完成后恢复父协程
    std::coroutine_handle<> await_suspend(std::coroutine_handle<>) const noexcept {
        if (mPrevious)  // 如果有上一个协程, 则返回上一个协程
            return mPrevious;
        else  // 如果没有上一个协程, 则返回调用者
            return std::noop_coroutine();
    }

    void await_resume() const noexcept {}
};

struct Promise {
    auto initial_suspend() { return std::suspend_always(); }

    // 确保协程完成后恢复父协程
    auto final_suspend() noexcept { return PreviousAwaiter(mPrevious); }

    void unhandled_exception() { throw; }

    // 设置返回值并挂起
    auto yield_value(int ret) {
        mRetValue = ret;
        return std::suspend_always();
    }

    // 结束时清零返回值
    void return_void() { mRetValue = 0; }

    std::coroutine_handle<Promise> get_return_object() { return std::coroutine_handle<Promise>::from_promise(*this); }

    int mRetValue;
    std::coroutine_handle<> mPrevious = nullptr;
};

struct Task {
    using promise_type = Promise;

    Task(std::coroutine_handle<promise_type> coroutine) : mCoroutine(coroutine) {}

    Task(Task &&) = delete;

    ~Task() { mCoroutine.destroy(); }

    std::coroutine_handle<promise_type> mCoroutine;
};

struct WorldTask {
    using promise_type = Promise;

    WorldTask(std::coroutine_handle<promise_type> coroutine) : mCoroutine(coroutine) {}

    WorldTask(WorldTask &&) = delete;

    ~WorldTask() { mCoroutine.destroy(); }

    struct WorldAwaiter {
        bool await_ready() const noexcept { return false; }

        // 将当前协程 hello 保存到子协程 world 的 mPrevious 并返回 world 协程句柄
        // 实现协程切换 hello -> world
        std::coroutine_handle<> await_suspend(std::coroutine_handle<> coroutine) const noexcept {
            mCoroutine.promise().mPrevious = coroutine;
            return mCoroutine;
        }

        void await_resume() const noexcept {}

        std::coroutine_handle<promise_type> mCoroutine;
    };

    // 重载 co_await 操作符, 返回 WorldAwaiter 对象, 触发协程切换逻辑
    auto operator co_await() { return WorldAwaiter(mCoroutine); }

    std::coroutine_handle<promise_type> mCoroutine;
};

WorldTask world() {
    debug(), "world";
    co_yield 422;
    co_yield 444;
    co_return;
}

Task hello() {
    debug(), "hello 正在构建worldTask";
    WorldTask worldTask = world();
    debug(), "hello 构建完了worldTask，开始等待world";
    co_await worldTask;
    debug(), "hello得到world返回", worldTask.mCoroutine.promise().mRetValue;
    co_await worldTask;
    debug(), "hello得到world返回", worldTask.mCoroutine.promise().mRetValue;
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
    debug(), "main调用完了hello";  // 其实只创建了task对象，并没有真正开始执行
    while (!t.mCoroutine.done()) {
        t.mCoroutine.resume();
        debug(), "main得到hello结果为", t.mCoroutine.promise().mRetValue;
    }
    return 0;
}
