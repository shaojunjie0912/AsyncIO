#include <coroutine>
#include <iostream>

using namespace std;

struct CoRet {
    struct promise_type;
    coroutine_handle<promise_type> h_;  // h_.resume() 或 h_() 即可让协程恢复运行

    struct promise_type {
        suspend_never initial_suspend() { return {}; }
        suspend_never final_suspend() noexcept { return {}; }
        void unhandled_exception() {}
        CoRet get_return_object() { return {coroutine_handle<promise_type>::from_promise(*this)}; }
    };
};

struct Foo {};

CoRet Guess() { co_await Foo{}; }

int main() { return 0; }
