// co_await 版本 generator

#include <coroutine>
#include <exception>
#include <iostream>
#include <stdexcept>
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
        std::cout << "移动构造\n";
    }

    Generator& operator=(Generator&& other) noexcept {
        if (this != &other) {
            Destory();
            h_ = std::exchange(other.h_, {});
        }
        std::cout << "移动赋值\n";
        return *this;
    }

public:
    struct promise_type {
        T value_;                       // 序列值
        std::exception_ptr exception_;  // 异常指针
        bool ready_{false};             // 是否有值 && 值是否被消费

        Generator get_return_object() { return Generator{handle_type::from_promise(*this)}; }

        std::suspend_always initial_suspend() { return {}; }

        // NOTE: final_suspend() 必须是 noexcept
        std::suspend_always final_suspend() noexcept { return {}; }

        void unhandled_exception() { exception_ = std::current_exception(); }

        // 为 co_await 后面的 非 awaitable 的 expr 提供的转换函数
        std::suspend_always await_transform(T value) {
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

// 这个协程死循环无法退出, 依靠 Generator 析构函数释放资源
Generator<int> Sequence() {
    int i = 0;
    while (i < 5) {
        co_await i++;
    }
}

int main() {
    auto generator = Sequence();
    for (int i = 0; i < 10; ++i) {
        if (generator.HasNext()) {
            std::cout << generator.Next() << std::endl;
        } else {
            break;
        }
    }
}
