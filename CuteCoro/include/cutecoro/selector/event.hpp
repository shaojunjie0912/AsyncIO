#pragma once

#include <sys/epoll.h>

#include <cstdint>
#include <cutecoro/handle.hpp>

namespace cutecoro {

using Flags_t = uint32_t;

// 事件的封装类
struct Event {
    enum Flags : Flags_t {
        EVENT_READ = EPOLLIN,    // 读事件
        EVENT_WRITE = EPOLLOUT,  // 写事件
    };

    int fd;                  // 文件描述符
    Flags flags;             // 注册事件类型 (EVENT_READ/EVENT_WRITE)
    HandleInfo handle_info;  // 句柄信息 (句柄 ID + 句柄指针)
};

}  // namespace cutecoro
