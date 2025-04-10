#include <coroutine>
#include <iostream>

using namespace std;

// 协程的返回类型
struct CoRet {
    struct promise_type;
    coroutine_handle<promise_type> h_;  // h_.resume() 或 h_() 即可让协程恢复运行

    struct promise_type {
        // 协程体开始?
        suspend_never initial_suspend() { return {}; }

        // 协程体结束?
        suspend_never final_suspend() noexcept { return {}; }

        // 处理异常?
        void unhandled_exception() {}

        // 协程返回值?
        CoRet get_return_object() {
            // 构造了一个 CoRet 对象
            return {coroutine_handle<promise_type>::from_promise(*this)};
        }
    };
};

// co_await 后的等待体
struct Input {
    // 遇到 co_await 时, ready=false 就需要暂停
    bool await_ready() { return false; }

    // 即将暂停, 即将跳转前, 得到 handle, 做出一些行为
    // 输入是当前协程的 handle
    // h.promise() 返回的就是这个协程里面的 promise 对象
    void await_suspend(coroutine_handle<CoRet::promise_type> h) {
        // 1. 返回类型为 void: 执行权还给调用者(本例中就是还给 main)
        // 2. 返回类型为 handle: 执行权给别的协程
    }

    // 只有当协程需要返回值时才需要
    int await_resume() { return 0; }
};

CoRet Guess() {
    // -------- 一些编译器隐藏的行为 --------
    // CoRet::promise_type promise;
    // CoRet ret = promise.get_return_object();
    // co_await promise.initial_suspend();
    Input input;
    // co_await 等待结束时返回一个 int 型结果
    int g = co_await input;
    cout << "coroutine: You guess: " << g << endl;

    // -------- 一些编译器隐藏的行为 --------
    // co_await promise.final_suspend();

    // 这里没有显式 return 哦
}

int main() {
    auto ret = Guess();
    cout << "main: make a guess ..." << endl;
    ret.h_.resume();  // 获取协程的 handle 调用 resumue() 继续运行协程
    return 0;
}
