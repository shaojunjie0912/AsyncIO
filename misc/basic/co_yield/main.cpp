#include <coroutine>
#include <iostream>

using namespace std;

// 协程的返回类型
struct CoRet {
    struct promise_type;
    coroutine_handle<promise_type> h_;

    struct promise_type {
        int out_;  // 用于保存 co_yield 的输出值(就是用户猜大猜小)

        suspend_never initial_suspend() { return {}; }

        suspend_never final_suspend() noexcept { return {}; }

        void unhandled_exception() {}

        CoRet get_return_object() { return {coroutine_handle<promise_type>::from_promise(*this)}; }

        // 输入: co_yield 后面那个表达式
        // 输出: 将要 co_await 的对象
        // 想让当前协程在 co_yield 处暂停并将执行权返回调用者的话, 就用 suspend_always
        // 不想暂停就 suspend_never
        suspend_always yield_value(int r) {
            out_ = r;
            return {};
        }
    };
};

// co_await 后的等待体
struct Input {
    int& in_;  // 用户输入

    bool await_ready() { return false; }

    void await_suspend(coroutine_handle<CoRet::promise_type>) {}

    int await_resume() { return in_; }
};

// Guess 协程
CoRet Guess(int& guess) {
    // -------- 一些编译器隐藏的行为 --------
    // CoRet::promise_type promise;
    // CoRet ret = promise.get_return_object();
    // co_await promise.initial_suspend();

    int res_rnd = (rand() % 30) + 1;  // 结果随机数
    Input input{guess};
    int ret_await = co_await input;
    cout << "coroutine: you guess: " << ret_await << endl;
    co_yield (res_rnd > ret_await ? 1 : (res_rnd == ret_await ? 0 : -1));

    // -------- 一些编译器隐藏的行为 --------
    // co_await promise.yield_value(); // suspend_always 挂起协程, 回到调用者
    // co_await promise.final_suspend();
}

int main() {
    srand(time(nullptr));
    int guess;
    auto ret = Guess(guess);
    cout << "main: make a guess ..." << endl;
    guess = 20;
    ret.h_.resume();  // 从 co_await 继续执行协程, 经过 co_yield -> yield_value 后执行权还给 main
    cout << "main: result random is "
         << ((ret.h_.promise().out_ == 1) ? "larger"
                                          : ((ret.h_.promise().out_ == 0) ? "the same" : "smaller"))
         << endl;
    return 0;
}
