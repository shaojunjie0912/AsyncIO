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
    co_await loop.WaitEvent(ev);

    int result{0};
    socklen_t result_len = sizeof(result);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &result, &result_len) < 0) {
        // error, fail somehow, close socket
        co_return false;
    }

    co_return result == 0;
}

}  // namespace detail

inline Task<Stream> OpenConnection(std::string_view ip, uint16_t port) {
    addrinfo hints{.ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM};
    addrinfo *server_info{nullptr};
    auto service = std::to_string(port);
    // TODO: getaddrinfo is a blocking api
    if (int rv = getaddrinfo(ip.data(), service.c_str(), &hints, &server_info); rv != 0) {
        throw std::system_error(std::make_error_code(std::errc::address_not_available));
    }
    finally { freeaddrinfo(server_info); };

    int sockfd = -1;
    for (auto p = server_info; p != nullptr; p = p->ai_next) {
        if ((sockfd = ::socket(p->ai_family, p->ai_socktype | SOCK_NONBLOCK, p->ai_protocol)) ==
            -1) {
            continue;
        }
        socket::SetBlocking(sockfd, false);  // 设置非阻塞
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
