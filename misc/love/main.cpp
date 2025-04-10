#include <chrono>
#include <coroutine>
#include <future>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

// TODO: 要不要画个协程与协程之间的跳转图理解理解嘞?
// TODO: princess 的 wait for Prince 在主线程, 而 got coins 在 prince 协程所在的线程中

// NOTE: 原来 Task 类既是协程的 promise_type 也是协程的 awaiter
// NOTE: std::coroutine_handle<T> 可以隐式转换为 std::coroutine_handle<>

struct Task {
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;
    handle_type h_;
    std::future<void> t_;

    // -------------- promise_type --------------
    struct promise_type {
        int result_;
        std::coroutine_handle<> next_ = nullptr;  // next_ 需要能够指向任意类型的协程

        Task get_return_object() { return Task{handle_type::from_promise(*this), {}}; }

        // initial_suspend: suspend_always 协程开始时就挂起
        std::suspend_always initial_suspend() { return {}; }

        // 返回类型是一个 awaiter, await_ready 为 false 因此 prince 的 final_suspend 会挂起 prince 协程
        // 然后由于 await_suspend 返回类型是协程句柄, 因此会返回到 princess 协程, 而不是调用者
        auto final_suspend() noexcept {
            struct next_awaiter {
                promise_type* me;

                bool await_ready() noexcept { return false; }

                // 返回值是协程句柄类型则返回到另一个协程
                std::coroutine_handle<> await_suspend(handle_type h) noexcept {
                    if (h.promise().next_)
                        return h.promise().next_;  // NOTE: prince 协程句柄
                    else
                        return std::noop_coroutine();
                }

                void await_resume() noexcept {}
            };
            return next_awaiter{this};
        }

        void return_value(int i) { result_ = i; }  // result_ 存储协程 co_return 的返回值

        void unhandled_exception() {}
    };

    // -------------- awaiter --------------
    bool await_ready() { return false; }

    // 返回值是 void 则返回调用者(即从 princess 协程 -> main)
    void await_suspend(handle_type h) {
        // h: Princess 协程句柄(当前正在被挂起的协程)
        // h_: Prince 协程句柄(存储在表示 Prince 的 Task 对象中)

        // 将 Prince 协程的 next_ 字段设置为指向 Princess 协程
        h_.promise().next_ = h;
        // 创建异步任务, 当新线程中的 Prince 完成时恢复 Princess 协程
        t_ = std::async(std::launch::async, [&] { h_.resume(); });
    }

    int await_resume() { return h_.promise().result_; }
};

// 王子协程
Task Prince() {
    // initial_suspend
    int coins{0};
    std::this_thread::sleep_for(500ms);
    ++coins;
    std::cout << std::this_thread::get_id() << " [Prince]: - found treasure! " << std::endl;
    co_return coins;
    // final_suspend
}

// 公主协程
Task Princess(Task& prince) {
    // initial_suspend
    std::cout << std::this_thread::get_id() << " [Princess]: - wait for Prince. " << std::endl;
    int c = co_await prince;  // NOTE: 编译器调用 prince 对象的 awaiter 方法
    std::cout << std::this_thread::get_id() << " [Princess]: - got " << c << " coins." << std::endl;
    co_return 0;
    // final_suspend
}

int main() {
    auto prince = Prince();            // initial_suspend: suspend_always 被挂起
    auto princess = Princess(prince);  // initial_suspend: suspend_always 被挂起
    princess.h_.resume();              // 公主协程恢复: co_await prince

    while (!princess.h_.done()) {
        std::cout << std::this_thread::get_id() << " [main]: wait ...\n";
        std::this_thread::sleep_for(100ms);
    }
    std::cout << std::this_thread::get_id() << " [main]: done " << std::endl;
}
