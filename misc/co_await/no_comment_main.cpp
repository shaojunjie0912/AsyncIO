#include <coroutine>
#include <iostream>

using namespace std;

// 协程的返回类型
struct CoRet {
    struct promise_type;
    coroutine_handle<promise_type> h_;

    struct promise_type {
        suspend_never initial_suspend() { return {}; }

        suspend_never final_suspend() noexcept { return {}; }

        void unhandled_exception() {}

        CoRet get_return_object() { return {coroutine_handle<promise_type>::from_promise(*this)}; }
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

    Input input{guess};
    int ret_await = co_await input;
    cout << "coroutine: You guess: " << ret_await << endl;

    // -------- 一些编译器隐藏的行为 --------
    // co_await promise.final_suspend();
}

int main() {
    int guess;
    auto ret = Guess(guess);
    cout << "main: make a guess ..." << endl;
    guess = 20;
    ret.h_.resume();
    return 0;
}
