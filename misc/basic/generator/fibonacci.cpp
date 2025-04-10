// 序列生成器
#include <concepts>
#include <coroutine>
#include <cstdint>
#include <exception>
#include <iostream>

// NOTE: concepts requires 简写
// template <typename From>
//     requires std::convertible_to<From, T>
//             ↓
// template <std::convertible_to<T> From>

template <typename T>
struct Generator {
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;
    handle_type h_;

    struct promise_type {
        T value_;  // 存储结果
        std::exception_ptr exception_;

        Generator get_return_object() { return Generator{handle_type::from_promise(*this)}; }

        // 开始时挂起
        std::suspend_always initial_suspend() { return {}; }

        // 结束时挂起
        std::suspend_always final_suspend() noexcept { return {}; }

        void unhandled_exception() {
            exception_ = std::current_exception();  // 保存异常
        }

        // yield 挂起协程
        template <std::convertible_to<T> From>
        std::suspend_always yield_value(From&& from) {
            value_ = std::forward<From>(from);  // 保存结果至 promise.value_
            return {};
        }

        void return_void() {}
    };

    // Generator 构造函数
    Generator(handle_type h) : h_(h) {}

    // Generator 析构函数
    ~Generator() {
        h_.destroy();  // 销毁协程
    }

    explicit operator bool() {
        Fill();
        return !h_.done();
    }

    // 重载括号运算符, 获得 promise 存储的结果, 即输出
    T operator()() {
        Fill();
        full_ = false;
        return std::move(h_.promise().value_);  // 移动之前的结果, 置空 promise
    }

private:
    bool full_{false};  // promise 中的 value_ 是否有值

    void Fill() {
        if (!full_) {     // 只有 full_ 为 false 才恢复协程执行
            h_.resume();  // NOTE: resume() 原来可以放在里面
            if (h_.promise().exception_) {
                std::rethrow_exception(h_.promise().exception_);  // 在调用上下文中传播协程异常
            }
            full_ = true;
        }
    }
};

Generator<uint64_t> FibonacciSequence(unsigned n) {
    // initial_suspend() 协程挂起, 返回调用者 main
    if (n == 0) {
        co_return;
    }

    if (n > 94) {
        throw std::runtime_error("斐波那契序列过大，元素将会溢出。");
    }

    co_yield 0;

    if (n == 1) {
        co_return;
    }

    co_yield 1;

    if (n == 2) {
        co_return;
    }

    uint64_t a = 0;
    uint64_t b = 1;

    for (unsigned i = 2; i < n; i++) {
        uint64_t s = a + b;
        co_yield s;
        a = b;
        b = s;
    }
    // final_suspend() 协程挂起, 返回调用者 main
}

int main() {
    try {
        auto gen = FibonacciSequence(3);  // 最大值94，避免 uint64_t 溢出

        // NOTE: 循环停止条件: 先 Fill() 再判断 h_.done()
        // NOTE: 需要 final_suspend() 返回 suspend_always, 才能进行 done() 判断
        for (int j = 0; gen; j++) {
            std::cout << "fib(" << j << ") = " << gen() << '\n';
        }
    } catch (const std::exception& ex) {
        std::cerr << "发生了异常：" << ex.what() << '\n';
    } catch (...) {
        std::cerr << "未知异常。\n";
    }
}
