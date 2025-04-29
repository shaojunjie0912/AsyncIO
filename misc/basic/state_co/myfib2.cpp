// 基于状态机的斐波那契协程实现
// NOTE: 扩展版(调度器, 就绪标志, 异常, 优先级...)

#include <exception>
#include <iostream>

struct FibFrame {
    int state_{0};                 // 状态, 指示恢复位置
    int a_;                        // fib param1
    int b_;                        // fib param2
    int c_;                        // fib param3
    bool ready_{false};            // 协程是否准备完成
    std::exception_ptr p_except_;  // 保存异常指针(可通过 fib_frame.p_except_ 调用)
    int n_;                        // 协程额外参数
    int priority_{0};              // 优先级

    // FibFrame(int n) : n_(n) {}

    // 成员函数
    int Resume() {
        try {
            switch (state_) {
                case 0: {
                    state_ = 1;
                    goto s0;  // 协程恢复位置
                }
                case 1: {
                    state_ = 2;
                    goto s1;
                }
                case 2: {
                    state_ = 3;
                    goto s2;
                }
                case 3: {
                    goto s3;
                }
            }
        s0:
            a_ = b_ = 1;
            return a_;  // 第 1 次调用 Fib() 时返回第 1 个数字
        s1:
            return b_;  // 第 2 次调用 Fib() 时返回第 2 个数字
        s2:
            while (true) {
                c_ = a_ + b_;
                return c_;
            s3:
                a_ = b_;
                b_ = c_;
            }
        } catch (...) {
            p_except_ = std::current_exception();
        }
    }
};

int main() {
    using std::cout, std::endl;

    FibFrame fib{};

    // NOTE: 下面三行其实就相当于是协程调度器
    for (int i = 0; i < 10; ++i) {
        cout << fib.Resume() << endl;
    }
}