#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <chrono>
#include <coroutine>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <memory>

using namespace std::chrono_literals;

// epoll 单线程 - 协程版本
// 并发: 定时器(2s) + 监听标准输入

constexpr int kBufSize{1024};
constexpr int kMaxEvents{4};

// 协程任务类型
struct Task {
    struct promise_type {
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };

    std::coroutine_handle<promise_type> handle;

    Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    ~Task() {
        if (handle && !handle.done()) {
            handle.destroy();
        }
    }
};

// EPoll事件调度器
class EPollScheduler {
public:
    EPollScheduler() : ep_fd(epoll_create1(0)) {
        if (ep_fd < 0) {
            perror("epoll_create1");
            throw std::runtime_error("创建epoll失败");
        }
    }

    ~EPollScheduler() {
        close(ep_fd);
        for (auto& [fd, _] : waiters) {
            if (fd != STDIN_FILENO) {
                close(fd);
            }
        }
    }

    // EPoll事件等待器
    struct EPollAwaiter {
        int fd;
        EPollScheduler& scheduler;
        std::coroutine_handle<> handle;

        EPollAwaiter(int fd, EPollScheduler& scheduler)
            : fd(fd), scheduler(scheduler), handle(nullptr) {}

        bool await_ready() const { return false; }

        void await_suspend(std::coroutine_handle<> h) {
            handle = h;
            scheduler.registerEvent(fd, handle);
        }

        void await_resume() {}
    };

    // 注册事件
    void registerEvent(int fd, std::coroutine_handle<> handle) {
        epoll_event event;
        event.events = EPOLLIN;
        event.data.fd = fd;
        epoll_ctl(ep_fd, EPOLL_CTL_ADD, fd, &event);
        waiters[fd] = handle;
    }

    // 创建定时器
    int createTimer(int seconds) {
        int timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
        if (timer_fd < 0) {
            perror("timerfd_create");
            throw std::runtime_error("创建定时器失败");
        }

        itimerspec duration{.it_interval = {.tv_sec = seconds, .tv_nsec = 0},
                            .it_value = {.tv_sec = seconds, .tv_nsec = 0}};

        if (timerfd_settime(timer_fd, 0, &duration, nullptr) < 0) {
            perror("timerfd_settime");
            close(timer_fd);
            throw std::runtime_error("设置定时器失败");
        }

        return timer_fd;
    }

    // 等待事件（从fd获取事件并恢复相应协程）
    EPollAwaiter waitEvent(int fd) { return EPollAwaiter(fd, *this); }

    // 运行事件循环
    void Run() {
        epoll_event events[kMaxEvents];

        while (true) {
            int num_events = epoll_wait(ep_fd, events, kMaxEvents, -1);

            if (num_events < 0) {
                if (errno == EINTR) continue;
                perror("epoll_wait");
                break;
            }

            for (int i = 0; i < num_events; ++i) {
                int fd = events[i].data.fd;
                if (waiters.find(fd) != waiters.end()) {
                    auto handle = waiters[fd];
                    if (!handle.done()) {
                        handle.resume();
                    }
                }
            }
        }
    }

private:
    int ep_fd;
    std::map<int, std::coroutine_handle<>> waiters;
};

// 协程函数：处理标准输入
Task handleStdin(EPollScheduler& scheduler) {
    while (true) {
        co_await scheduler.waitEvent(STDIN_FILENO);

        char buf[kBufSize]{};
        read(STDIN_FILENO, buf, kBufSize);
        std::cout << "\t\t[Stdin 协程]: " << buf << std::flush;
    }
}

// 协程函数：处理定时器
Task handleTimer(EPollScheduler& scheduler, int timer_fd) {
    while (true) {
        co_await scheduler.waitEvent(timer_fd);

        uint64_t expirations;
        read(timer_fd, &expirations, sizeof(expirations));  // 必须读取
        std::cout << "[Timer 协程]: 2s" << std::endl;
    }
}

int main() {
    EPollScheduler scheduler;

    // 创建并注册定时器协程
    int timer_fd = scheduler.createTimer(2);
    auto timer_task = handleTimer(scheduler, timer_fd);

    // 创建并注册标准输入协程
    auto stdin_task = handleStdin(scheduler);

    // 运行事件循环
    std::cout << "启动协程调度器，请输入文本..." << std::endl;
    scheduler.Run();

    return 0;
}
