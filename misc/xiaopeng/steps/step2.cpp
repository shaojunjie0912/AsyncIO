
#include <chrono>
#include <coroutine>

#include "debug.hpp"

struct PreviousAwaiter {
    std::coroutine_handle<> mPrevious;

    bool await_ready() const noexcept { return false; }

    // 如果存了前一个协程句柄, 就返回到前一个协程
    // 如果没有, 则挂起但不恢复任何协程? 控制权立即回到调用当前协程 resume 的地方?
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> coroutine) const noexcept {
        if (mPrevious)
            return mPrevious;
        else
            return std::noop_coroutine();
    }

    void await_resume() const noexcept {}
};

struct Promise {
    // 刚开始时总是暂停
    auto initial_suspend() { return std::suspend_always(); }

    // 当结束时, (如果mPrevious有值则)返回到前一个协程
    auto final_suspend() noexcept { return PreviousAwaiter(mPrevious); }

    void unhandled_exception() { throw; }

    // co_yield 存储值后就挂起, 因为返回了 suspend_always 所以执行权返回给 main
    // NOTE: co_yield 挂起时不会自动使用 await_suspend 中保存的 mPrevious 这个 hello 协程的句柄
    // 显然得精确指定才行
    auto yield_value(int ret) {
        mRetValue = ret;
        return std::suspend_always();
    }

    void return_void() { mRetValue = 0; }

    std::coroutine_handle<Promise> get_return_object() { return std::coroutine_handle<Promise>::from_promise(*this); }

    int mRetValue;
    std::coroutine_handle<> mPrevious = nullptr;  // 保存前一个协程的句柄(后面用于恢复前一个协程)
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

        // 执行权从 coroutine 对应的 hello 协程转移给 mCoroutine 对应的 world 协程
        // 不用担心回不去因为已经将 hello 协程句柄保存到 world 协程 Promise 对象中
        std::coroutine_handle<> await_suspend(std::coroutine_handle<> coroutine) const noexcept {
            mCoroutine.promise().mPrevious = coroutine;
            return mCoroutine;
        }
        // TODO: 现在这是精确指定了 world 协程句柄,
        // 待会儿试试返回 void 是不是也一样能执行 world, 都没必要返回 world 的协程句柄?

        void await_resume() const noexcept {}

        std::coroutine_handle<promise_type> mCoroutine;
    };

    auto operator co_await() { return WorldAwaiter(mCoroutine); }

    std::coroutine_handle<promise_type> mCoroutine;
};

WorldTask world() {
    debug(), "world";
    co_yield 422;  // 这个值只是存到了 WorldTask 的 Promise 中
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
