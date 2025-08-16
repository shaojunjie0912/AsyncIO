#pragma once

#include <fmt/core.h>
#include <sys/types.h>

#include <asyncio/detail/concepts/awaitable.hpp>
#include <asyncio/finally.hpp>
#include <asyncio/scheduled_task.hpp>
#include <asyncio/stream.hpp>
#include <list>

namespace asyncio {

namespace concepts {

template <typename CONNECT_CB>
concept ConnectCb = requires(CONNECT_CB cb) {
    { cb(std::declval<Stream>()) } -> concepts::Awaitable;
};

}  // namespace concepts

inline constexpr size_t max_connect_count = 16;

template <concepts::ConnectCb CONNECT_CB>
struct Server : NonCopyable {
    Server(CONNECT_CB cb, int fd) : connect_cb_(cb), listenfd_(fd) {}

    Server(Server&& other)
        : connect_cb_(other.connect_cb_), listenfd_{std::exchange(other.listenfd_, -1)} {}

    ~Server() { Close(); }

    Task<void> ServeForever() {
        Event ev{.fd = listenfd_, .flags = Event::Flags::EVENT_READ};
        auto ev_awaiter = GetEventLoop().WaitEvent(ev);
        std::list<ScheduledTask<Task<>>> connected;
        while (true) {
            sockaddr_storage remoteaddr{};  // 对端地址信息
            socklen_t addrlen = sizeof(remoteaddr);
            co_await ev_awaiter;
            // 非阻塞式 accept
            int connfd_ = ::accept(listenfd_, reinterpret_cast<sockaddr*>(&remoteaddr), &addrlen);
            if (connfd_ == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 继续等待连接
                    continue;
                } else {
                    // 处理 accept 错误
                    throw std::system_error(errno, std::system_category());
                }
            }
            // 将处理新连接的回调函数 (connect_cb_) 作为协程任务添加到事件循环中
            connected.emplace_back(schedule_task(connect_cb_(Stream{connfd_, remoteaddr})));
            CleanUpConnected(connected);
        }
    }

private:
    // 用于垃圾回收，移除已经处理完毕的客户端连接任务。
    void CleanUpConnected(std::list<ScheduledTask<Task<>>>& connected) {
        if (connected.size() < 100) [[likely]] {
            return;
        }
        for (auto iter = connected.begin(); iter != connected.end();) {
            if (iter->IsDone()) {
                // 即使不关心返回值，也需要调用 GetResult() 来处理可能发生的异常
                iter->GetResult();
                iter = connected.erase(iter);
            } else {
                ++iter;  // 任务未完成, 继续检查下一个
            }
        }
    }

private:
    // 关闭监听套接字
    void Close() {
        if (listenfd_ > 0) {
            ::close(listenfd_);
        }
        listenfd_ = -1;
    }

private:
    [[no_unique_address]] CONNECT_CB connect_cb_;  // 处理新连接的回调函数
    int listenfd_{-1};                             // 监听套接字
};

/**
 * @brief
 *
 * @tparam CONNECT_CB 处理新连接的回调函数类型
 * @param connect_cb 处理新连接的回调函数
 * @param ip 监听的 IP 地址
 * @param port 监听的端口号
 * @return Task<Server<CONNECT_CB>>
 */
template <concepts::ConnectCb CONNECT_CB>
Task<Server<CONNECT_CB>> StartServer(CONNECT_CB connect_cb, std::string_view ip, uint16_t port) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;      // 不限制地址类型 (ipv4/6)
    hints.ai_socktype = SOCK_STREAM;  // 只返回支持 TCP 协议的地址

    addrinfo* server_info{nullptr};
    auto service = std::to_string(port);
    // TODO: getaddrinfo is a blocking api
    if (int rv = getaddrinfo(ip.data(), service.c_str(), &hints, &server_info); rv != 0) {
        throw std::system_error(std::make_error_code(std::errc::address_not_available));
    }
    finally { freeaddrinfo(server_info); };

    int listenfd = -1;
    for (auto p = server_info; p != nullptr; p = p->ai_next) {
        // NOTE: 1. socket
        if ((listenfd = ::socket(p->ai_family, p->ai_socktype | SOCK_NONBLOCK, p->ai_protocol)) ==
            -1) {
            continue;
        }
        socket::SetBlocking(listenfd, false);  // 设置监听套接字为非阻塞
        int yes = 1;
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));  // 允许地址重用
        // NOTE: 2. bind
        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        } else {
            close(listenfd);
            listenfd = -1;
        }
    }
    if (listenfd == -1) {
        throw std::system_error(std::make_error_code(std::errc::address_not_available));
    }

    // NOTE: 3. listen
    if (listen(listenfd, max_connect_count) == -1) {
        throw std::system_error(errno, std::system_category());
    }

    // 使用 cb 和 listenfd 创建 Server 对象
    co_return Server{connect_cb, listenfd};
}

}  // namespace asyncio
