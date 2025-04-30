// co_yield 版本 generator

#include <coroutine>
#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <utility>

// TODO: 只知道必须 move-only 表示协程句柄与一个协程绑定, 但是还没试过拷贝

// 耗尽异常
class ExhaustedException : std::exception {};

template <typename T>
struct Generator {
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

private:
    handle_type h_;  // 协程句柄

public:
    // handle_type 类似指针, 按值传递即可
    explicit Generator(handle_type h) : h_(h) {}

    ~Generator() noexcept { Destory(); }

    // move-only
    Generator(Generator const&) = delete;

    Generator& operator=(Generator const&) = delete;

    Generator(Generator&& other) noexcept : h_(std::exchange(other.h_, {})) {
        // std::cout << "移动构造\n";
    }

    Generator& operator=(Generator&& other) noexcept {
        if (this != &other) {
            Destory();
            h_ = std::exchange(other.h_, {});
        }
        // std::cout << "移动赋值\n";
        return *this;
    }

public:
    struct promise_type {
        T value_;                       // 序列值
        std::exception_ptr exception_;  // 异常指针
        bool ready_{false};             // 是否有值 && 值是否被消费

        Generator get_return_object() { return Generator{handle_type::from_promise(*this)}; }

        // NOTE: suspend_always/never 无所谓了, 后面 HasNext 会判断决定 resume()
        std::suspend_never initial_suspend() { return {}; }

        // NOTE: final_suspend() 必须是 noexcept
        std::suspend_always final_suspend() noexcept { return {}; }

        void unhandled_exception() { exception_ = std::current_exception(); }

        std::suspend_always yield_value(T value) {
            value_ = value;
            ready_ = true;
            return {};
        }
    };

public:
    T Next() {
        if (HasNext()) {  // 如果有值, 且没有被消费, 则消费一个值, 重置 ready
            h_.promise().ready_ = false;
            return h_.promise().value_;
        } else {
            throw ExhaustedException();
        }
    }

    // 处理两个逻辑
    // 1. 是否有值(没有就恢复协程)
    // 2. 如果有值, 是否已经被消费
    bool HasNext() {
        if (!h_ || h_.done()) {  // 协程被销毁/已经结束
            return false;
        }

        if (!h_.promise().ready_) {  // 如果下一个值还没准备好, 则恢复执行
            h_.resume();
        }

        if (h_.done()) {  // 恢复执行后, 协程执行完了, 那 co_await 一定没传出值
            return false;
        } else {  // 恢复执行后, 协程再次来到 co_await 则有值了, 需要消费了
            return true;
        }
    }

public:
    // 把外部的协程拿到里面来了

    // static 方法, 生成一个 Generator 对象
    template <typename... Args>
    static Generator Gen(Args... args) {
        ((co_yield args), ...);  // 逗号表达式 参数包折叠
    }

    // 将当前的 Generator 映射为另一个不同类型的 Generator
    template <typename Func>
    Generator<std::invoke_result_t<Func, T>> Map(Func fn) {
        auto up_stream = std::move(*this);  // TODO: 转移句柄所有权?
        while (up_stream.HasNext()) {
            co_yield fn(up_stream.Next());
        }
    }

    template <typename CoroFunc>
    std::invoke_result<CoroFunc, T> FlatMap(CoroFunc fn) {
        auto upstream = std::move(*this);
        while (upstream.HasNext()) {
            auto generator = fn(upstream.Next());  // 值映射成新的 Generator
            while (generator.HasNext()) {          // 将新的 Generator 展开
                co_yield generator.Next();
            }
        }
    }

    template <typename F>
    void ForEach(F f) {
        while (HasNext()) {
            f(Next());
        }
    }

public:
    void Print() {
        while (true) {
            if (HasNext()) {
                std::cout << Next() << " ";
            } else {
                break;
            }
        }
        std::cout << '\n';
    }

public:
    void Destory() {
        if (h_) {
            h_.destroy();
        }
    }

    bool done() {
        if (h_) {
            return h_.done();
        } else {
            throw std::runtime_error("协程已经销毁啦! 调用方法可能产生未定义行为!");
        }
    }
};

Generator<int> Fibonacci(int size) {
    size -= 2;
    co_yield 0;
    co_yield 1;
    int a = 0;
    int b = 1;
    while (size--) {
        int s{a + b};
        co_yield s;
        a = b;
        b = s;
    }
}

int main() {
    auto gen1 = Generator<int>::Gen(1, 2, 3, 4);
    gen1.Print();

    auto gen2 = Fibonacci(10);
    gen2.Print();

    auto gen2_str = Fibonacci(5).Map([](int i) { return std::to_string(i); });
    gen2_str.Print();

    auto gen3_str = Generator<int>::Gen(1, 2, 3, 4).Map([](int i) { return std::to_string(i); });
    gen3_str.Print();

    // Generator<int>::Gen(1, 2, 3, 4)
    //     // 返回值类型必须显式写出来，表明这个函数是个协程
    //     .FlatMap([](auto i) -> Generator<int> {
    //         for (int j = 0; j < i; ++j) {
    //             // 在协程当中，我们可以使用 co_yield 传值出来
    //             co_yield j;
    //         }
    //     })
    //     .ForEach([](auto i) {
    //         if (i == 0) {
    //             std::cout << std::endl;
    //         }
    //         std::cout << "* ";
    //     });
}
