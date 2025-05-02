#pragma once

#include <sys/socket.h>
#include <sys/types.h>

//
#include <cutecoro/detail/selector/event.hpp>
#include <cutecoro/finally.hpp>
#include <cutecoro/stream.hpp>
#include <cutecoro/task.hpp>

namespace cutecoro {

namespace detail {

// 异步连接一个给定的非阻塞 fd 到指定的地址 addr
// 1. connect 非阻塞返回 EINPROGRESS
// 2. epoll_wait 等待 socket 可写
// 3. getsockopt 检查 connect 是否成功
Task<bool> Connect(int fd, const sockaddr *addr, socklen_t len) noexcept;

}  // namespace detail

// 异步打开一个到指定 ip 地址和端口 port 的 TCP 连接
// NOTE: 这里可能输入的是域名而不是 ip, 因此需要 getaddrinfo, 且同时支持 ipv4 和 ipv6
Task<Stream> OpenConnection(std::string_view ip, uint16_t port);

}  // namespace cutecoro
