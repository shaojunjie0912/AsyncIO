#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>

using namespace std::chrono_literals;

// epoll 单线程
// 并发: 定时器(2s) + 监听标准输入

constexpr int kBufSize{1024};
constexpr int kMaxEvents{4};

using EventCallback = std::function<void()>;

// Stdin 事件对应回调
void HandleStdin() {
    char buf[kBufSize]{};
    read(STDIN_FILENO, buf, kBufSize);
    std::cout << "\t\t[Stdin]: " << buf << std::flush;
}

// 注册 Stdin 监听事件
void RegisterStdin(int ep_fd) {
    epoll_event event;
    event.events = EPOLLIN;
    event.data.ptr = reinterpret_cast<void*>(HandleStdin);
    epoll_ctl(ep_fd, EPOLL_CTL_ADD, STDIN_FILENO, &event);
}

int timer_fd;

// Timer 事件对应回调
void HandleTimer() {
    uint64_t expirations;
    read(timer_fd, &expirations, sizeof(expirations));  // 必须读取
    std::cout << "[Timer]: 2s\n";
}

timespec Second2Timespec(int second) { return timespec{.tv_sec = second, .tv_nsec = 0}; }

// 注册 Timer 监听事件
void RegisterTimer(int ep_fd, int seconds) {
    // 创建 timer_fd
    timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    itimerspec duration{.it_interval = Second2Timespec(seconds), .it_value = Second2Timespec(seconds)};
    timerfd_settime(timer_fd, 0, &duration, nullptr);

    // 注册 epoll
    epoll_event event;
    event.events = EPOLLIN;
    event.data.ptr = reinterpret_cast<void*>(HandleTimer);
    epoll_ctl(ep_fd, EPOLL_CTL_ADD, timer_fd, &event);
}

int main() {
    int const ep_fd{epoll_create1(0)};

    RegisterStdin(ep_fd);
    RegisterTimer(ep_fd, 2);

    while (true) {
        epoll_event ep_events[kMaxEvents];
        int const num_events = epoll_wait(ep_fd, ep_events, kMaxEvents, -1);  // timeout=-1 一直阻塞
        if (num_events < 0) {
            perror("epoll_wait");
            continue;
        }
        for (int i{0}; i < num_events; ++i) {
            // void* 转为函数指针 void (*)()
            auto handler_ptr{reinterpret_cast<void (*)()>(ep_events[i].data.ptr)};
            handler_ptr();
        }
    }
    return 0;
}
