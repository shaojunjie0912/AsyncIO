#pragma once

#include <thread>

#include "epoll_loop.hpp"
#include "timer_loop.hpp"

namespace co_async {

struct AsyncLoop {
    void Run() {
        while (true) {
            auto timeout = mTimerLoop.Run();
            if (mEpollLoop.hasEvent()) {
                mEpollLoop.Run(timeout);
            } else if (timeout) {
                std::this_thread::sleep_for(*timeout);
            } else {
                break;
            }
        }
    }

    operator TimerLoop&() { return mTimerLoop; }

    operator EpollLoop&() { return mEpollLoop; }

private:
    TimerLoop mTimerLoop;
    EpollLoop mEpollLoop;
};

}  // namespace co_async
