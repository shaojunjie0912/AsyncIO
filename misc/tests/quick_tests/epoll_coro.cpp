#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <coroutine>
#include <functional>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

// 错误处理辅助函数
void die(const char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// 设置socket为非阻塞模式
void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) die("fcntl F_GETFL");
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) die("fcntl F_SETFL O_NONBLOCK");
}

// 表示一个可等待的网络IO事件
class IoEvent {
public:
    bool ready = false;
    int fd;
    int events;
    int result = 0;

    IoEvent(int fd, int events) : fd(fd), events(events) {}

    // 设置事件已就绪并保存结果
    void set_ready(int res) {
        ready = true;
        result = res;
    }

    // 重置事件状态
    void reset() {
        ready = false;
        result = 0;
    }
};

// 协程任务类
class Task {
public:
    struct promise_type {
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };

    Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    ~Task() {
        if (handle && !handle.done()) {
            // 注意：通常应该在handle.done()后再destroy，但这里为了简化处理
            handle.destroy();
        }
    }

    // 恢复协程执行
    void resume() {
        if (handle && !handle.done()) {
            handle();
        }
    }

    // 禁用拷贝
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    // 允许移动
    Task(Task&& other) noexcept : handle(other.handle) { other.handle = nullptr; }

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle) handle.destroy();
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }

private:
    std::coroutine_handle<promise_type> handle;
};

// Epoll服务器类
class EpollServer {
public:
    EpollServer(int port, int max_events = 64)
        : port(port), max_events(max_events), epoll_events(max_events) {
        // 创建epoll实例
        epoll_fd = epoll_create1(0);
        if (epoll_fd == -1) die("epoll_create1");

        // 创建并配置监听socket
        listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd == -1) die("socket");

        int opt = 1;
        if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
            die("setsockopt");

        set_nonblocking(listen_fd);

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) die("bind");

        if (listen(listen_fd, SOMAXCONN) == -1) die("listen");

        // 将监听socket添加到epoll中
        add_to_epoll(listen_fd, EPOLLIN | EPOLLET);

        std::cout << "Server started on port " << port << std::endl;
    }

    ~EpollServer() {
        close(epoll_fd);
        close(listen_fd);
    }

    // 向epoll实例添加文件描述符
    void add_to_epoll(int fd, uint32_t events) {
        struct epoll_event ev;
        ev.events = events;
        ev.data.fd = fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) die("epoll_ctl:add");
    }

    // 修改epoll中的文件描述符事件
    void mod_epoll(int fd, uint32_t events) {
        struct epoll_event ev;
        ev.events = events;
        ev.data.fd = fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) die("epoll_ctl:mod");
    }

    // 从epoll中移除文件描述符
    void del_from_epoll(int fd) {
        if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr) == -1) die("epoll_ctl:del");
    }

    // 等待IO事件的可等待对象
    class IoAwaiter {
    public:
        IoAwaiter(EpollServer& server, std::shared_ptr<IoEvent> event)
            : server(server), event(event) {}

        bool await_ready() { return event->ready; }

        void await_suspend(std::coroutine_handle<> handle) {
            // 保存协程句柄以便稍后恢复
            server.waiters[event->fd] = handle;

            // 确保我们正在监视正确的事件
            server.mod_epoll(event->fd, event->events | EPOLLET);
        }

        int await_resume() {
            // 移除等待者
            server.waiters.erase(event->fd);

            // 返回IO操作的结果
            int result = event->result;
            event->reset();
            return result;
        }

    private:
        EpollServer& server;
        std::shared_ptr<IoEvent> event;
    };

    // 协程:处理客户端连接
    Task handle_client(int client_fd) {
        char buffer[4096];
        set_nonblocking(client_fd);

        // 添加到epoll
        add_to_epoll(client_fd, EPOLLIN | EPOLLET);

        try {
            while (true) {
                // 创建读取事件
                auto read_event = std::make_shared<IoEvent>(client_fd, EPOLLIN);

                // 等待读取数据
                int bytes_read = co_await IoAwaiter(*this, read_event);

                if (bytes_read <= 0) {
                    // 客户端关闭连接或出错
                    break;
                }

                // 创建写事件
                auto write_event = std::make_shared<IoEvent>(client_fd, EPOLLOUT);

                // 等待可以写入
                int bytes_written = co_await IoAwaiter(*this, write_event);

                if (bytes_written <= 0) {
                    // 写入失败
                    break;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error handling client: " << e.what() << std::endl;
        }

        // 清理
        std::cout << "Client disconnected: " << client_fd << std::endl;
        del_from_epoll(client_fd);
        close(client_fd);

        co_return;
    }

    // 协程:接受新连接
    Task accept_connections() {
        while (true) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);

            int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 没有新连接，等待下一个accept事件
                    auto accept_event = std::make_shared<IoEvent>(listen_fd, EPOLLIN);
                    co_await IoAwaiter(*this, accept_event);
                    continue;
                } else {
                    // 真正的错误
                    std::cerr << "Error accepting connection: " << strerror(errno) << std::endl;
                    break;
                }
            }

            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
            std::cout << "New connection from " << client_ip << ":" << ntohs(client_addr.sin_port)
                      << ", fd: " << client_fd << std::endl;

            // 创建新协程处理这个客户端
            tasks.push_back(handle_client(client_fd));
        }

        co_return;
    }

    // 运行服务器的主循环
    void Run() {
        // 启动接受连接的协程
        tasks.push_back(accept_connections());

        // 事件循环
        while (true) {
            // 等待事件
            int nfds = epoll_wait(epoll_fd, epoll_events.data(), max_events, -1);
            if (nfds == -1) {
                die("epoll_wait");
            }

            // 处理就绪的事件
            for (int i = 0; i < nfds; i++) {
                int fd = epoll_events[i].data.fd;
                uint32_t events = epoll_events[i].events;

                if (fd == listen_fd) {
                    // 有新连接到达
                    if (waiters.count(fd)) {
                        auto event = std::make_shared<IoEvent>(fd, EPOLLIN);
                        event->set_ready(1);  // 表示有连接就绪
                        waiters[fd]();        // 恢复等待此fd的协程
                    }
                } else {
                    // 客户端socket就绪
                    if (waiters.count(fd)) {
                        int result = 0;

                        if (events & EPOLLIN) {
                            // 可读事件
                            char buffer[4096];
                            result = read(fd, buffer, sizeof(buffer));

                            if (result > 0) {
                                // 读取成功，打印接收到的数据
                                std::cout << "Received " << result << " bytes from fd " << fd
                                          << std::endl;
                                buffer[result] = '\0';  // 确保以null结尾
                                std::cout << "Data: " << buffer << std::endl;

                                // 回显数据
                                write(fd, buffer, result);
                            }
                        }

                        if (events & EPOLLOUT) {
                            // 可写事件
                            result = 1;  // 表示可以写入
                        }

                        // 设置事件为就绪并恢复协程
                        auto event = std::make_shared<IoEvent>(fd, events);
                        event->set_ready(result);
                        waiters[fd]();  // 恢复等待此fd的协程
                    }

                    // 处理错误和关闭事件
                    if ((events & EPOLLERR) || (events & EPOLLHUP)) {
                        if (waiters.count(fd)) {
                            auto event = std::make_shared<IoEvent>(fd, events);
                            event->set_ready(-1);  // 表示错误
                            waiters[fd]();         // 恢复协程以处理错误
                        } else {
                            // 没有等待的协程，直接关闭
                            close(fd);
                        }
                    }
                }
            }
        }
    }

private:
    int port;
    int max_events;
    int epoll_fd;
    int listen_fd;
    std::vector<epoll_event> epoll_events;
    std::vector<Task> tasks;
    std::unordered_map<int, std::coroutine_handle<>> waiters;
};

int main() {
    // 创建服务器实例在端口8080上
    EpollServer server(8080);

    // 运行服务器
    server.Run();

    return 0;
}
