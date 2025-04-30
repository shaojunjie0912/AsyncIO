#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <coroutine>
#include <iostream>
#include <map>
#include <memory>
#include <vector>

// 设置文件描述符为非阻塞模式
void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// 协程任务调度器
class Scheduler {
private:
    int epoll_fd_;
    std::map<int, std::coroutine_handle<>> fd_to_coro_;
    bool running_ = false;

public:
    Scheduler() {
        epoll_fd_ = epoll_create1(0);
        if (epoll_fd_ < 0) {
            std::cerr << "创建epoll失败: " << strerror(errno) << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    ~Scheduler() { close(epoll_fd_); }

    // 注册文件描述符和关联的协程
    void register_fd(int fd, std::coroutine_handle<> h, uint32_t events = EPOLLIN | EPOLLET) {
        epoll_event ev;
        ev.events = events;
        ev.data.fd = fd;

        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
            std::cerr << "添加fd到epoll失败: " << strerror(errno) << std::endl;
            return;
        }

        fd_to_coro_[fd] = h;
    }

    // 修改文件描述符的事件
    void modify_fd(int fd, uint32_t events) {
        if (fd_to_coro_.find(fd) == fd_to_coro_.end()) {
            return;
        }

        epoll_event ev;
        ev.events = events;
        ev.data.fd = fd;

        if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) < 0) {
            std::cerr << "修改fd事件失败: " << strerror(errno) << std::endl;
        }
    }

    // 取消注册文件描述符
    void unregister_fd(int fd) {
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
            std::cerr << "从epoll移除fd失败: " << strerror(errno) << std::endl;
        }

        fd_to_coro_.erase(fd);
    }

    // 运行调度器事件循环
    void Run() {
        running_ = true;
        const int MAX_EVENTS = 64;
        struct epoll_event events[MAX_EVENTS];

        while (running_) {
            int n = epoll_wait(epoll_fd_, events, MAX_EVENTS, -1);

            if (n < 0) {
                if (errno == EINTR) continue;
                std::cerr << "epoll_wait失败: " << strerror(errno) << std::endl;
                break;
            }

            for (int i = 0; i < n; ++i) {
                int fd = events[i].data.fd;
                auto it = fd_to_coro_.find(fd);

                if (it != fd_to_coro_.end()) {
                    auto handle = it->second;
                    if (handle && !handle.done()) {
                        handle.resume();
                    }
                }
            }
        }
    }

    void stop() { running_ = false; }
};

// 全局调度器实例
Scheduler g_scheduler;

// 用于等待fd就绪的可等待对象
template <uint32_t Events>
struct AwaitableFd {
    int fd;

    AwaitableFd(int fd) : fd(fd) {}

    bool await_ready() { return false; }

    void await_suspend(std::coroutine_handle<> h) { g_scheduler.register_fd(fd, h, Events); }

    void await_resume() { g_scheduler.unregister_fd(fd); }
};

// 使用协程表示的任务
struct Task {
    struct promise_type {
        Task get_return_object() { return {}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {
            std::cerr << "协程中捕获到未处理异常" << std::endl;
            std::terminate();
        }
    };
};

// 处理客户端连接的协程
Task handle_client(int client_fd) {
    char buffer[4096];
    ssize_t bytes_read;

    try {
        while (true) {
            // 等待客户端数据
            co_await AwaitableFd<EPOLLIN | EPOLLET>(client_fd);

            bool should_close = false;

            // 由于使用边缘触发模式，需要循环读取直到EAGAIN
            while (true) {
                bytes_read = read(client_fd, buffer, sizeof(buffer));

                if (bytes_read < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // 没有更多数据可读
                        break;
                    } else {
                        // 读取错误
                        std::cerr << "读取客户端数据错误: " << strerror(errno) << std::endl;
                        should_close = true;
                        break;
                    }
                } else if (bytes_read == 0) {
                    // 客户端关闭连接
                    std::cout << "客户端断开连接 (fd: " << client_fd << ")" << std::endl;
                    should_close = true;
                    break;
                } else {
                    // 处理接收到的数据
                    buffer[bytes_read] = '\0';
                    std::cout << "从客户端接收到 " << bytes_read << " 字节: " << buffer
                              << std::endl;

                    // 回显数据给客户端
                    co_await AwaitableFd<EPOLLOUT | EPOLLET>(client_fd);
                    send(client_fd, buffer, bytes_read, 0);
                }
            }

            if (should_close) {
                break;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "处理客户端连接异常: " << e.what() << std::endl;
    }

    close(client_fd);
    std::cout << "客户端连接处理完成 (fd: " << client_fd << ")" << std::endl;
}

// 接受新连接的协程
Task accept_connections(int server_fd) {
    try {
        while (true) {
            // 等待新连接
            co_await AwaitableFd<EPOLLIN>(server_fd);

            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);

            int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd < 0) {
                std::cerr << "接受连接失败: " << strerror(errno) << std::endl;
                continue;
            }

            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            std::cout << "新连接来自 " << client_ip << ":" << ntohs(client_addr.sin_port)
                      << " (fd: " << client_fd << ")" << std::endl;

            // 设置客户端socket为非阻塞
            set_nonblocking(client_fd);

            // 启动处理客户端连接的协程
            handle_client(client_fd);
        }
    } catch (const std::exception& e) {
        std::cerr << "接受连接协程异常: " << e.what() << std::endl;
    }
}

int main() {
    try {
        // 创建服务器socket
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            std::cerr << "创建socket失败: " << strerror(errno) << std::endl;
            return 1;
        }

        // 设置地址复用
        int opt = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            std::cerr << "设置socket选项失败: " << strerror(errno) << std::endl;
            return 1;
        }

        // 绑定地址和端口
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(8080);

        if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
            std::cerr << "绑定地址失败: " << strerror(errno) << std::endl;
            return 1;
        }

        // 监听连接
        if (listen(server_fd, SOMAXCONN) < 0) {
            std::cerr << "监听失败: " << strerror(errno) << std::endl;
            return 1;
        }

        // 设置服务器socket为非阻塞
        set_nonblocking(server_fd);

        std::cout << "协程服务器启动，监听端口: 8080" << std::endl;

        // 启动接受连接的协程
        accept_connections(server_fd);

        // 运行调度器
        g_scheduler.Run();

        close(server_fd);
    } catch (const std::exception& e) {
        std::cerr << "服务器异常: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
