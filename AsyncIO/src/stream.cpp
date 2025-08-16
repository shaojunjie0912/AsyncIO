#include <asyncio/stream.hpp>

namespace asyncio {

namespace socket {

bool SetBlocking(int fd, bool blocking) {
    if (fd < 0) {
        return false;
    }
    // SOCK_NONBLOCK: 创建套接字时就能设置非阻塞模式
    // 不需要调用 ioctl 了 (但做了创建时设置好的假设)
    if constexpr (SOCK_NONBLOCK != 0) {
        return true;
    } else {
#if defined(_WIN32)
        unsigned long block = !blocking;
        return !ioctlsocket(fd, FIONBIO, &block);
#elif __has_include(<sys/ioctl.h>) && defined(FIONBIO)
        unsigned int block = !blocking;  // bool -> int
        // FIONBIO: 阻塞/非阻塞
        // block: 0 阻塞 / 1 非阻塞
        return !ioctl(fd, FIONBIO, &block);
#else
        int delay_flag, new_delay_flag;
        delay_flag = fcntl(fd, F_GETFL, 0);
        if (delay_flag == -1) {
            return false;
        }
        new_delay_flag = blocking ? (delay_flag & ~O_NONBLOCK) : (delay_flag | O_NONBLOCK);
        if (new_delay_flag != delay_flag) {
            return !fcntl(fd, F_SETFL, new_delay_flag);
        } else {
            return false;
        }
#endif
    }
}

}  // namespace socket

const void* GetInAddr(const sockaddr* sa) {
    if (sa->sa_family == AF_INET) {
        return &reinterpret_cast<const sockaddr_in*>(sa)->sin_addr;
    } else {
        return &reinterpret_cast<const sockaddr_in6*>(sa)->sin6_addr;
    }
}

uint16_t GetInPort(const sockaddr* sa) {
    if (sa->sa_family == AF_INET) {
        return ntohs(reinterpret_cast<const sockaddr_in*>(sa)->sin_port);
    } else {
        return ntohs(reinterpret_cast<const sockaddr_in6*>(sa)->sin6_port);
    }
}

}  // namespace asyncio