#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <functional>
#include <iostream>
#include <unordered_map>
#include <vector>

// 设置文件描述符为非阻塞模式
void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

class EpollServer {
private:
    int server_fd_;
    int epoll_fd_;
    bool running_;
    std::unordered_map<int, std::function<void(int)>> read_callbacks_;

public:
    // 构造函数，初始化服务器
    EpollServer(int port) : running_(false) {
        // 创建服务器socket
        server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd_ < 0) {
            std::cerr << "创建socket失败: " << strerror(errno) << std::endl;
            exit(EXIT_FAILURE);
        }

        // 设置地址复用
        int opt = 1;
        if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            std::cerr << "设置socket选项失败: " << strerror(errno) << std::endl;
            exit(EXIT_FAILURE);
        }

        // 绑定地址和端口
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        if (bind(server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
            std::cerr << "绑定地址失败: " << strerror(errno) << std::endl;
            exit(EXIT_FAILURE);
        }

        // 监听连接
        if (listen(server_fd_, SOMAXCONN) < 0) {
            std::cerr << "监听失败: " << strerror(errno) << std::endl;
            exit(EXIT_FAILURE);
        }

        // 创建epoll实例
        epoll_fd_ = epoll_create1(0);
        if (epoll_fd_ < 0) {
            std::cerr << "创建epoll失败: " << strerror(errno) << std::endl;
            exit(EXIT_FAILURE);
        }

        // 设置服务器socket为非阻塞
        set_nonblocking(server_fd_);

        // 将服务器socket添加到epoll
        struct epoll_event event;
        event.events = EPOLLIN;
        event.data.fd = server_fd_;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_fd_, &event) < 0) {
            std::cerr << "添加服务器socket到epoll失败: " << strerror(errno) << std::endl;
            exit(EXIT_FAILURE);
        }

        std::cout << "服务器初始化完成，监听端口: " << port << std::endl;
    }

    // 析构函数，清理资源
    ~EpollServer() {
        if (server_fd_ >= 0) close(server_fd_);
        if (epoll_fd_ >= 0) close(epoll_fd_);
    }

    // 处理新连接
    void handle_new_connection() {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            std::cerr << "接受连接失败: " << strerror(errno) << std::endl;
            return;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        std::cout << "新连接来自 " << client_ip << ":" << ntohs(client_addr.sin_port) << " (fd: " << client_fd << ")"
                  << std::endl;

        // 设置客户端socket为非阻塞
        set_nonblocking(client_fd);

        // 将客户端socket添加到epoll
        struct epoll_event event;
        event.events = EPOLLIN | EPOLLET;  // 边缘触发模式
        event.data.fd = client_fd;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &event) < 0) {
            std::cerr << "添加客户端socket到epoll失败: " << strerror(errno) << std::endl;
            close(client_fd);
            return;
        }

        // 设置客户端的读回调函数
        read_callbacks_[client_fd] = [this](int fd) { handle_client_read(fd); };
    }

    // 处理客户端数据
    void handle_client_read(int fd) {
        char buffer[4096];
        ssize_t bytes_read;

        // 由于使用边缘触发模式，需要循环读取直到EAGAIN
        while (true) {
            bytes_read = read(fd, buffer, sizeof(buffer));

            if (bytes_read < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 没有更多数据可读
                    break;
                } else {
                    // 读取错误
                    std::cerr << "读取客户端数据错误: " << strerror(errno) << std::endl;
                    close_client(fd);
                    return;
                }
            } else if (bytes_read == 0) {
                // 客户端关闭连接
                std::cout << "客户端断开连接 (fd: " << fd << ")" << std::endl;
                close_client(fd);
                return;
            } else {
                // 处理接收到的数据
                buffer[bytes_read] = '\0';
                std::cout << "从客户端接收到 " << bytes_read << " 字节: " << buffer << std::endl;

                // 回显数据给客户端
                send(fd, buffer, bytes_read, 0);
            }
        }
    }

    // 关闭客户端连接
    void close_client(int fd) {
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
        close(fd);
        read_callbacks_.erase(fd);
    }

    // 启动服务器
    void start() {
        running_ = true;
        std::cout << "服务器启动，等待连接..." << std::endl;

        const int MAX_EVENTS = 64;
        struct epoll_event events[MAX_EVENTS];

        while (running_) {
            int nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, -1);

            if (nfds < 0) {
                if (errno == EINTR) {
                    // 被信号中断，继续
                    continue;
                }
                std::cerr << "epoll_wait失败: " << strerror(errno) << std::endl;
                break;
            }

            for (int i = 0; i < nfds; ++i) {
                int fd = events[i].data.fd;

                if (fd == server_fd_) {
                    // 新连接
                    handle_new_connection();
                } else if (events[i].events & EPOLLIN) {
                    // 客户端可读事件
                    auto it = read_callbacks_.find(fd);
                    if (it != read_callbacks_.end()) {
                        it->second(fd);
                    }
                }
            }
        }
    }

    // 停止服务器
    void stop() { running_ = false; }
};

int main() {
    try {
        EpollServer server(8080);
        server.start();
    } catch (const std::exception& e) {
        std::cerr << "服务器异常: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
