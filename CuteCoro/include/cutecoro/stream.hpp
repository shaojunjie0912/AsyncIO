#pragma once

#include <fcntl.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <utility>
#include <vector>

//
#include <cutecoro/detail/noncopyable.hpp>
#include <cutecoro/detail/selector/event.hpp>
#include <cutecoro/event_loop.hpp>
#include <cutecoro/task.hpp>

namespace cutecoro {

namespace socket {

// 设置文件描述符 fd 为阻塞/非阻塞模式
bool SetBlocking(int fd, bool blocking);

}  // namespace socket

// 网络流
struct Stream : NonCopyable {
    using Buffer = std::vector<char>;  // 经典 Buffer

    // 构造函数 1: 接收一个已连接的文件描述符 fd
    // 通过 getsockname 存储本地套接字的地址信息 (ip & port)
    Stream(int fd) : read_fd_(fd), write_fd_(dup(fd)) {
        if (read_fd_ >= 0) {
            socklen_t addrlen = sizeof(sock_info_);
            // 获取 ip 和 port 存入 sock_info_
            getsockname(read_fd_, reinterpret_cast<sockaddr*>(&sock_info_), &addrlen);
        }
    }

    // 构造函数 2: 接收一个文件描述符 fd 和包含对端地址信息的 sockinfo
    Stream(int fd, const sockaddr_storage& sockinfo)
        : read_fd_(fd), write_fd_(dup(fd)), sock_info_(sockinfo) {}

    Stream(Stream&& other)
        : read_fd_{std::exchange(other.read_fd_, -1)},
          write_fd_{std::exchange(other.write_fd_, -1)},
          read_event_{std::exchange(other.read_event_, {})},
          write_event_{std::exchange(other.write_event_, {})},
          read_awaiter_{std::move(other.read_awaiter_)},
          write_awaiter_{std::move(other.write_awaiter_)},
          sock_info_{other.sock_info_} {}

    ~Stream() { Close(); }

public:
    void Close() {
        read_awaiter_.Destroy();
        write_awaiter_.Destroy();
        if (read_fd_ > 0) {
            ::close(read_fd_);
        }
        if (write_fd_ > 0) {
            ::close(write_fd_);
        }
        read_fd_ = -1;
        write_fd_ = -1;
    }

    /**
     * @brief 异步读取数据
     *
     * @param sz 读取的字节数, 默认 -1 读取到 EOF
     * @return Task<Buffer>
     */
    Task<Buffer> Read(ssize_t sz = -1) {
        if (sz < 0) {  // 如果 sz < 0 (默认), 则读取直到 EOF
            co_return co_await ReadUntilEof();
        }

        Buffer result(sz, 0);
        co_await read_awaiter_;  // 等待直到可读
        sz = ::read(read_fd_, result.data(), result.size());
        if (sz == -1) {
            throw std::system_error(errno, std::system_category());
        }
        result.resize(sz);
        co_return result;
    }

    Task<> Write(const Buffer& buf) {
        ssize_t total_write = 0;
        while (total_write < static_cast<ssize_t>(buf.size())) {
            co_await write_awaiter_;
            ssize_t sz = ::write(write_fd_, buf.data() + total_write, buf.size() - total_write);
            if (sz == -1) {
                throw std::system_error(errno, std::system_category());
            }
            total_write += sz;
        }
        co_return;
    }

    /**
     * @brief 获取套接字地址信息
     *
     * @return sockaddr_storage const&
     */
    sockaddr_storage const& GetSockInfo() const { return sock_info_; }

private:
    Task<Buffer> ReadUntilEof() {
        Buffer result(chunk_size, 0);

        int current_read = 0;
        int total_read = 0;

        do {
            co_await read_awaiter_;
            // 保证下一次读取时有足够空间容纳 chunk_size 大小的数据块
            if (result.size() < total_read + chunk_size) {
                result.resize(total_read + chunk_size);  // 确保读之前有足够空间
            }
            current_read = ::read(read_fd_, result.data() + total_read, chunk_size);
            if (current_read == -1) {
                // 非阻塞 IO 是否因为当前资源暂时不可用而失败
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    current_read = 1;  // > 0 保持循环继续进行
                    continue;
                }
                throw std::system_error(errno, std::system_category());
            }
            total_read += current_read;
        } while (current_read > 0);  // 当 current_read == 0 时, EOF 结束循环

        result.resize(total_read);  // 修剪大小
        co_return result;
    }

private:
    int read_fd_{-1};
    int write_fd_{-1};

    Event read_event_{.fd = read_fd_, .flags = Event::EVENT_READ};     // 读就绪事件
    Event write_event_{.fd = write_fd_, .flags = Event::EVENT_WRITE};  // 写就绪事件

    // 可等待对象 (等待读就绪事件)
    EventLoop::WaitEventAwaiter read_awaiter_{GetEventLoop().WaitEvent(read_event_)};
    // 可等待对象 (等待写就绪事件)
    EventLoop::WaitEventAwaiter write_awaiter_{GetEventLoop().WaitEvent(write_event_)};

    sockaddr_storage sock_info_{};              // 通用套接字地址结构, 兼容 IPv4&6
    constexpr static size_t chunk_size = 4096;  // 每次读操作的块大小 (缓冲区大小) 4KB
};

/**
 * @brief 获取 sockaddr 的地址
 *
 * @param sa
 * @return const void*
 */
const void* GetInAddr(const sockaddr* sa);

/**
 * @brief 获取 sockaddr 的端口号
 *
 * @param sa
 * @return uint16_t
 */
uint16_t GetInPort(const sockaddr* sa);

}  // namespace cutecoro
