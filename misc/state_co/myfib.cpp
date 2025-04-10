// 基于状态机的斐波那契协程实现

#include <iostream>

struct FibFrame {
    int state_ = 0;
    int a_;
    int b_;
    int c_;
};

int Fib(FibFrame& frame) {
    switch (frame.state_) {
        case 0: {
            frame.state_ = 1;
            goto s0;  // 协程恢复位置
        }
        case 1: {
            frame.state_ = 2;
            goto s1;
        }
        case 2: {
            frame.state_ = 3;
            goto s2;
        }
        case 3: {
            goto s3;
        }
    }
s0:
    frame.a_ = frame.b_ = 1;
    return frame.a_;  // 第 1 次调用 Fib() 时返回第 1 个数字
s1:
    return frame.b_;  // 第 2 次调用 Fib() 时返回第 2 个数字
s2:
    while (true) {
        frame.c_ = frame.a_ + frame.b_;
        return frame.c_;
    s3:
        frame.a_ = frame.b_;
        frame.b_ = frame.c_;
    }
}

int main() {
    FibFrame frame{};
    using std::cout, std::endl;
    for (int i = 0; i < 10; ++i) {
        cout << Fib(frame) << endl;
    }
    return 0;
}