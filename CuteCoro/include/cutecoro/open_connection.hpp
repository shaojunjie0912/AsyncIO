#pragma once

#include <sys/socket.h>
#include <sys/types.h>

#include <system_error>
//
#include <cutecoro/finally.hpp>
#include <cutecoro/selector/event.hpp>
#include <cutecoro/stream.hpp>
#include <cutecoro/task.hpp>

namespace cutecoro {

namespace detail {

// 异步连接一个给定的非阻塞 fd 到指定的地址 addr
// 1. connect 非阻塞返回 EINPROGRESS
// 2. epoll_wait 等待 socket 可写
// 3. getsockopt 检查 connect 是否成功
inline Task<bool> Connect(int fd, const sockaddr *addr, socklen_t len) noexcept {
    int rc = ::connect(fd, addr, len);  // 非阻塞式 connect

    if (rc == 0) {  // 连接成功
        co_return true;
    }

    if (rc < 0 && errno != EINPROGRESS) {  // 其他错误
        throw std::system_error(errno, std::system_category());
    }

    // rc == -1 且 errno == EINPROGRESS: 连接正在进行中
    Event ev{.fd = fd, .flags = Event::Flags::EVENT_WRITE};  // 监听可写事件 (可写代表连接已建立)
    auto &loop = GetEventLoop();
    co_await loop.WaitEvent(ev);  // TODO: 等待事件循环通知该套接字变可写

    // 检查 fd 上是否存在挂起错误
    int result{0};
    socklen_t result_len = sizeof(result);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &result, &result_len) < 0) {
        // error, fail somehow, close socket
        co_return false;
    }

    co_return result == 0;
}

}  // namespace detail

// 异步打开一个到指定 ip 地址和端口 port 的 TCP 连接
// NOTE: 这里可能输入的是域名而不是 ip, 因此需要 getaddrinfo, 且同时支持 ipv4 和 ipv6
inline Task<Stream> OpenConnection(std::string_view ip, uint16_t port) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;      // 不限制地址类型 (ipv4/6)
    hints.ai_socktype = SOCK_STREAM;  // 只返回支持 TCP 协议的地址

    addrinfo *server_info{nullptr};
    auto service = std::to_string(port);

    // TODO: getaddrinfo is a blocking api
    // ==================================================================================
    // node: 主机名/IP
    // service: 服务名/端口号字符串
    // hints: 提示结构体 (指定想要的地址类型)
    // res: 返回地址信息链表 (指向一个或多个 addrinfo 结果)
    // int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints,
    //                 struct addrinfo **res);
    // ==================================================================================
    if (int rv = getaddrinfo(ip.data(), service.c_str(), &hints, &server_info); rv != 0) {
        throw std::system_error(std::make_error_code(std::errc::address_not_available));
    }
    finally { freeaddrinfo(server_info); };  // go defer 函数退出时执行

    int sockfd = -1;
    for (auto p = server_info; p != nullptr; p = p->ai_next) {
        if ((sockfd = ::socket(p->ai_family, p->ai_socktype | SOCK_NONBLOCK, p->ai_protocol)) ==
            -1) {
            continue;
        }
        socket::SetBlocking(sockfd, false);  // 设置非阻塞 (二次了)
        if (co_await detail::Connect(sockfd, p->ai_addr, p->ai_addrlen)) {
            break;
        }
        close(sockfd);
        sockfd = -1;
    }

    if (sockfd == -1) {
        throw std::system_error(std::make_error_code(std::errc::address_not_available));
    }

    co_return Stream{sockfd};
}

}  // namespace cutecoro
