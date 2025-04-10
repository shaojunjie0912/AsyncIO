#include <coroutine>
#include <future>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct CoRet {
    struct promise_type {
        int in_;   // 用户输入
        int out_;  // co_yield 输出, 即用户猜大猜小
        int res_;  // co_return 输出, 即用户猜的次数

        std::suspend_never initial_suspend() { return {}; }

        std::suspend_always final_suspend() noexcept { return {}; }

        void unhandled_exception() {}

        CoRet get_return_object() { return {std::coroutine_handle<promise_type>::from_promise(*this)}; }

        std::suspend_always yield_value(int r) {
            out_ = r;
            return {};
        }

        void return_value(int r) { res_ = r; }
    };

    std::coroutine_handle<promise_type> _h;  // _h.resume(), _h()
};

struct Input {
    int* p_in_;   // 用户输入
    int* p_out_;  //

    bool await_ready() { return false; }

    // 协程挂起前, 做出的行为
    // await_suspend 返回值是 void, 则协程挂起后返回调用者
    void await_suspend(std::coroutine_handle<CoRet::promise_type> h) {
        p_in_ = &h.promise().in_;    // 将等待体 Input 的 p_in_ 指向 promise 的 in_
        p_out_ = &h.promise().out_;  // 将等待体 Input 的 p_out_ 指向 promise 的 out_
    }

    int await_resume() { return *p_in_; }
};

CoRet Guess() {
    // co_await promise.initial_suspend();
    int answer = (rand() % 30) + 1;  // 1 ~ 30 随机数
    Input input;
    int num_guess = 0;  // 猜数字的次数
    while (true) {
        // NOTE: Guess 里面只有 Input, 没法获取 handle.promise() 所以才绑定 promise 和 Input
        // 这样用户在外部可以 get/set promise 的值, 也反映到 Input 里面
        int guess = co_await input;  // co_await 等待用户输入?

        ++num_guess;
        (*input.p_out_) = (answer > guess ? 1 : (answer == guess ? 0 : -1));
        if ((*input.p_out_) == 0) {
            co_return num_guess;
        }
    }
    // co_await promise.final_suspend();...
}

struct Hasher {
    size_t operator()(const std::pair<int, int>& p) const {
        return (size_t)(p.first << 8) + (size_t)(p.second);
    }
};

int main() {
    srand(time(nullptr));

    // 创建 100 个 Guess 协程, 初始放入一个范围为 [1,30] 的桶中
    std::unordered_map<std::pair<int, int>, std::vector<CoRet>, Hasher> buckets;
    for (auto i = 0; i < 100; ++i) {
        buckets[{1, 30}].push_back(Guess());
    }

    while (!buckets.empty()) {
        auto it = buckets.begin();
        auto& range = it->first;
        auto& handles = it->second;

        int g = (range.first + range.second) / 2;       // 中间值
        auto ur = std::make_pair(g + 1, range.second);  // 大于中间值的范围
        auto lr = std::make_pair(range.first, g - 1);   // 小于中间值的范围

        std::vector<std::future<int>> cmp;
        cmp.reserve(handles.size());
        for (auto& coret : handles) {
            // 在不同的线程中执行协程, 并保存输出结果(猜大还是猜小)
            // TODO: 这里 async 不如用线程池
            cmp.push_back(std::async(std::launch::async, [&coret, g] {
                coret._h.promise().in_ = g;
                coret._h.resume();
                return coret._h.promise().out_;
            }));
        }
        // 循环 100 次
        for (int i = 0; i < (int)handles.size(); ++i) {
            int r = cmp[i].get();  // 创建一个新线程, 执行 async 中的协程, 并获得返回值
            if (r == 0) {
                std::cout << "The secret number is " << handles[i]._h.promise().in_ << ", total # guesses is "
                          << handles[i]._h.promise().res_ << std::endl;
            } else if (r == 1)
                buckets[ur].push_back(handles[i]);
            else {
                buckets[lr].push_back(handles[i]);
            }
        }
        buckets.erase(it);
    }

    // auto ret = Guess();
    // pair<int, int> range = {1,30};
    // int in, out;
    // do
    // {
    //     in = (range.first+range.second)/2;
    //     ret._h.promise()._in = in;
    //     cout << "main: make a guess: " << ret._h.promise()._in << endl;

    //     ret._h.resume(); // resume from co_await

    //     out = ret._h.promise()._out;
    //     cout << "main: result is " <<
    //     ((out == 1) ? "larger" :
    //     ((out == 0) ? "the same" : "smaller"))
    //         << endl;
    //     if(out == 1) range.first = in+1;
    //     else if(out == -1) range.second = in-1;
    // }
    // while(out != 0);
}
