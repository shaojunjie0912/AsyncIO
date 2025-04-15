#pragma once

#include <fmt/core.h>
#include <unistd.h>

#include <cutecoro/selector/event.hpp>
#include <vector>

namespace cutecoro {

// epoll 操作封装类
struct EpollSelector {
    EpollSelector() : epfd_(epoll_create1(0)) {
        if (epfd_ < 0) {
            perror("epoll_create1");
            throw;
        }
    }

    // 注册事件
    void register_event(Event const& event) {
        epoll_event ev;
        ev.events = event.flags;
        ev.data.ptr = const_cast<HandleInfo*>(&event.handle_info);  // NOTE: const_cast 去除常量属性
        if (epoll_ctl(epfd_, EPOLL_CTL_ADD, event.fd, &ev) == 0) {
            ++register_event_count_;
        }
    }

    // 移除事件
    void remove_event(const Event& event) {
        epoll_event ev;
        ev.events = event.flags;
        if (epoll_ctl(epfd_, EPOLL_CTL_DEL, event.fd, &ev) == 0) {
            --register_event_count_;
        }
    }

    // 就是抽象的 poll 操作
    std::vector<Event> select(int timeout /* ms */) {
        // errno = 0;
        std::vector<epoll_event> events;
        events.resize(register_event_count_);
        int num_events = epoll_wait(epfd_, events.data(), register_event_count_, timeout);
        std::vector<Event> result;
        for (int i = 0; i < num_events; ++i) {
            auto handle_info =
                reinterpret_cast<HandleInfo*>(events[i].data.ptr);  // void* -> HandleInfo*
            if (handle_info->handle != nullptr &&
                handle_info->handle != (Handle*)&handle_info->handle) {
                Event event;
                event.handle_info = *handle_info;  // TODO: 只有回调指针有用? fd 和 flags 没用?
                result.push_back(std::move(event));
            } else {
                // NOTE: 特殊处理: 表示有事件发生, 但是句柄信息的句柄指针代表没有相应回调需要处理
                handle_info->handle = (Handle*)&handle_info->handle;
            }
        }
        return result;
    }

    ~EpollSelector() {
        if (epfd_ > 0) {
            close(epfd_);
        }
    }

    bool is_stop() { return register_event_count_ == 1; }

private:
    int epfd_;                     // epoll 文件描述符
    int register_event_count_{1};  // 注册事件数量
};

}  // namespace cutecoro
