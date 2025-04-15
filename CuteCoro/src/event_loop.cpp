#include <cutecoro/event_loop.hpp>

namespace cutecoro {

EventLoop& get_event_loop() {
    static EventLoop event_loop;
    return event_loop;
}

void EventLoop::run_until_complete() {
    while (!is_stop()) {
        run_once();
    }
}

void EventLoop::cleanup_delayed_call() {
    // 移除被取消的回调
    while (!schedule_.empty()) {
        auto&& [when, handle_info] = schedule_[0];
        if (auto iter = cancelled_.find(handle_info.id); iter != cancelled_.end()) {
            std::ranges::pop_heap(schedule_, std::ranges::greater{}, &TimerHandle::first);
            schedule_.pop_back();
            cancelled_.erase(iter);
        } else {
            break;
        }
    }
}

void EventLoop::run_once() {
    std::optional<MSDuration> timeout;  // 调用 selector_.select() 的最大阻塞时间: ms
    if (!ready_.empty()) {              // 就绪队列非空,
        timeout.emplace(0);
    } else if (!schedule_.empty()) {
        auto&& [when, _] = schedule_[0];  // 最小堆的堆顶: 过期时间最早
        timeout = std::max(when - time(), MSDuration(0));
    }

    // 这里如果 timeout = 0 那就直接不阻塞了
    // 如果 timeout > 0 那么就会阻塞一会获取事件, 然后 schedule_ 中任务就 ready 了
    auto event_lists = selector_.select(timeout.has_value() ? timeout->count() : -1);

    // NOTE: 范围 for 循环中 auto&& 是万能引用
    for (auto&& event : event_lists) {
        ready_.push(event.handle_info);  // 把这次 epoll_wait 监听到的发生事件对应的回调加入 ready_
    }

    auto end_time = time();
    while (!schedule_.empty()) {
        // 最小堆的堆顶: 过期时间最早 (刚刚 epoll_wait 了这个时间)
        auto&& [when, handle_info] = schedule_[0];
        if (when >= end_time) {  // 如果遇到了还没过期的, 就退出循环
            break;
        }
        ready_.push(handle_info);  // 把过期的加入 ready_ 马上执行
        std::ranges::pop_heap(schedule_, std::ranges::greater{}, &TimerHandle::first);  // 去除堆顶
        schedule_.pop_back();                                                           // 去除堆顶
    }

    // 提前获取 ready_ 大小, 因为循环内会改变
    for (size_t ntodo = ready_.size(), i = 0; i < ntodo; ++i) {
        auto [handle_id, handle] = ready_.front();
        ready_.pop();
        // 如果当前 handle 是应该取消的, 那么就从 cancelled_ 中移除, 并跳过执行
        if (auto iter = cancelled_.find(handle_id); iter != cancelled_.end()) {
            cancelled_.erase(iter);
            continue;
        }
        handle->set_state(Handle::UNSCHEDULED);
        handle->run();
    }

    cleanup_delayed_call();
}

}  // namespace cutecoro
