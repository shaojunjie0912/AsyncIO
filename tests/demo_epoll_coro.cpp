#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <cerrno>
#include <coroutine>
#include <cstring>
#include <iostream>

// 全局 epoll 实例
int epoll_fd;

// 简单的协程任务类型
struct Task {
    struct promise_type {
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_never initial_suspend() {
            return {};
        }
        // 为了保证协程结束后可以正常退出，我们采用不挂起 final_suspend
        std::suspend_never final_suspend() noexcept {
            return {};
        }
        void return_void() {}
        void unhandled_exception() {
            std::terminate();
        }
    };

    std::coroutine_handle<promise_type> handle;
    Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    ~Task() {
        if (handle) handle.destroy();
    }
};

// 自定义等待器，用于注册 fd 到 epoll，并在事件到达时恢复协程
struct EpollAwaiter {
    int fd;
    uint32_t event_mask;             // 例如 EPOLLIN、EPOLLOUT 等
    std::coroutine_handle<> handle;  // 保存协程句柄

    // 表示该等待器总是需要等待（返回 false）
    bool await_ready() {
        return false;
    }

    // await_suspend 中将当前协程注册到 epoll 上
    void await_suspend(std::coroutine_handle<> h) {
        handle = h;
        epoll_event ev;
        ev.events = event_mask;
        // 将当前等待器的地址传给 epoll，便于事件回调中恢复协程
        ev.data.ptr = this;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
            std::cerr << "epoll_ctl 添加失败: " << strerror(errno) << std::endl;
            std::terminate();
        }
    }

    // resume 前移除该 fd 的注册，避免重复触发
    void await_resume() {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
    }
};

// 示例协程：等待 fd 可读，然后执行读取操作
Task read_coroutine(int fd) {
    std::cout << "协程等待 fd " << fd << " 的可读事件..." << std::endl;
    co_await EpollAwaiter{fd, EPOLLIN};
    std::cout << "fd " << fd << " 已可读，开始读取数据..." << std::endl;

    char buffer[1024];
    ssize_t n = read(fd, buffer, sizeof(buffer) - 1);
    if (n > 0) {
        buffer[n] = '\0';
        std::cout << "读取到数据: " << buffer << std::endl;
    } else if (n < 0) {
        std::cerr << "读取错误: " << strerror(errno) << std::endl;
    }
    co_return;
}

int main() {
    // 创建 epoll 实例
    epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        std::cerr << "创建 epoll 失败" << std::endl;
        return -1;
    }

    // 此处以标准输入（STDIN_FILENO）为例，将其设置为非阻塞模式
    int fd = STDIN_FILENO;
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    // 启动协程
    auto task = read_coroutine(fd);

    // epoll 事件循环
    constexpr int MAX_EVENTS = 10;
    epoll_event events[MAX_EVENTS];
    while (true) {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (n < 0) {
            std::cerr << "epoll_wait 错误: " << strerror(errno) << std::endl;
            break;
        }
        for (int i = 0; i < n; i++) {
            // 通过 data.ptr 获取对应的等待器，并恢复挂起的协程
            auto* awaiter = static_cast<EpollAwaiter*>(events[i].data.ptr);
            if (awaiter && awaiter->handle) {
                awaiter->handle.resume();
            }
        }
    }

    close(epoll_fd);
    return 0;
}
