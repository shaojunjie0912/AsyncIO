#pragma once

#include <sys/epoll.h>

#include <cstdint>
#include <asyncio/handle.hpp>

namespace asyncio {

using Flags_t = uint32_t;

// 事件的封装类
struct Event {
    enum Flags : Flags_t {
        EVENT_NONE = 0,          // 空事件
        EVENT_READ = EPOLLIN,    // 读就绪事件
        EVENT_WRITE = EPOLLOUT,  // 写就绪事件
    };

    int fd{-1};                // 文件描述符
    Flags flags{EVENT_NONE};   // 注册事件类型 (EVENT_READ/EVENT_WRITE)
    HandleInfo handle_info{};  // 句柄信息 (句柄 ID + 句柄指针) 即回调
};

}  // namespace asyncio
