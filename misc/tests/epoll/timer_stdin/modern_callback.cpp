#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>  // 添加此行以支持 std::exchange
#include <vector>

using namespace std::chrono_literals;

class EpollEventManager {
public:
    using EventCallback = std::function<void()>;

    struct EventHandler {
        EventCallback callback;
        int fd = -1;
    };

    EpollEventManager() : epoll_fd_{epoll_create1(0)} {
        if (epoll_fd_ < 0) {
            throw std::runtime_error("Failed to create epoll file descriptor");
        }
    }

    ~EpollEventManager() {
        if (epoll_fd_ >= 0) {
            close(epoll_fd_);
        }

        // 关闭所有注册的定时器文件描述符
        for (const auto& [fd, _] : handlers_) {
            if (fd != STDIN_FILENO && fd >= 0) {
                close(fd);
            }
        }
    }

    // 仅支持移动的类
    EpollEventManager(const EpollEventManager&) = delete;
    EpollEventManager& operator=(const EpollEventManager&) = delete;

    // 修复的移动构造函数 - 避免使用 std::exchange
    EpollEventManager(EpollEventManager&& other) noexcept
        : epoll_fd_(other.epoll_fd_), handlers_(std::move(other.handlers_)) {
        other.epoll_fd_ = -1;  // 将被移动对象设置为有效状态
    }

    // 修复的移动赋值运算符 - 避免使用 std::exchange
    EpollEventManager& operator=(EpollEventManager&& other) noexcept {
        if (this != &other) {
            if (epoll_fd_ >= 0) {
                close(epoll_fd_);
            }
            epoll_fd_ = other.epoll_fd_;
            other.epoll_fd_ = -1;  // 将被移动对象设置为有效状态
            handlers_ = std::move(other.handlers_);
        }
        return *this;
    }

    void RegisterStdin(EventCallback callback) {
        auto handler_ptr =
            std::make_shared<EventHandler>(EventHandler{.callback = std::move(callback), .fd = STDIN_FILENO});

        epoll_event event{};
        event.events = EPOLLIN;
        event.data.ptr = handler_ptr.get();

        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, STDIN_FILENO, &event) < 0) {
            throw std::runtime_error("Failed to register stdin with epoll");
        }

        handlers_[STDIN_FILENO] = std::move(handler_ptr);
    }

    void RegisterTimer(std::chrono::seconds interval, EventCallback callback) {
        // 创建定时器文件描述符
        int timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
        if (timer_fd < 0) {
            throw std::runtime_error("Failed to create timer file descriptor");
        }

        // 设置定时器间隔
        itimerspec duration{};
        duration.it_interval.tv_sec = interval.count();
        duration.it_value.tv_sec = interval.count();

        if (timerfd_settime(timer_fd, 0, &duration, nullptr) < 0) {
            close(timer_fd);
            throw std::runtime_error("Failed to set timer");
        }

        // 创建一个带有定时器文件描述符和回调的处理器
        auto handler_ptr = std::make_shared<EventHandler>(
            EventHandler{.callback =
                             [user_cb = std::move(callback), timer_fd]() {
                                 uint64_t expirations;
                                 read(timer_fd, &expirations, sizeof(expirations));  // 必须读取以重置
                                 user_cb();
                             },
                         .fd = timer_fd});

        // 注册到 epoll
        epoll_event event{};
        event.events = EPOLLIN;
        event.data.ptr = handler_ptr.get();

        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, timer_fd, &event) < 0) {
            close(timer_fd);
            throw std::runtime_error("Failed to register timer with epoll");
        }

        // 存储处理器以便后续清理
        handlers_[timer_fd] = std::move(handler_ptr);
    }

    void EventLoop() {
        constexpr int kMaxEvents{10};
        std::vector<epoll_event> events(kMaxEvents);

        while (true) {
            int num_events = epoll_wait(epoll_fd_, events.data(), kMaxEvents, -1);

            if (num_events < 0) {
                if (errno == EINTR) {
                    continue;  // 被信号中断，重试
                }
                throw std::runtime_error("epoll_wait failed: " + std::string(strerror(errno)));
            }

            for (int i = 0; i < num_events; ++i) {
                auto* handler_ptr = static_cast<EventHandler*>(events[i].data.ptr);
                handler_ptr->callback();
            }
        }
    }

private:
    int epoll_fd_{-1};
    std::unordered_map<int, std::shared_ptr<EventHandler>> handlers_;
};

int main() {
    try {
        EpollEventManager event_manager;

        constexpr int kBufSize{1024};

        // 注册标准输入处理器
        event_manager.RegisterStdin([]() {
            char buf[kBufSize]{};
            read(STDIN_FILENO, buf, kBufSize);
            std::cout << "\t\t[Stdin]: " << buf << '\n';
        });

        // 注册定时器（2秒）
        event_manager.RegisterTimer(2s, []() { std::cout << "[Timer]: 2s\n"; });

        // 启动事件循环
        event_manager.EventLoop();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
