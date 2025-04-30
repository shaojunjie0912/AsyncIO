#pragma once

#include <algorithm>
#include <chrono>
#include <coroutine>
#include <cutecoro/handle.hpp>
#include <cutecoro/noncopyable.hpp>
#include <cutecoro/selector/selector.hpp>
#include <queue>
#include <unordered_set>

namespace cutecoro {

class EventLoop : NonCopyable {
    // NOTE: 事件循环内部推荐用 duration 相对时间
    // 只关心"距离启动多久后触发", 不关心绝对时间
    using MSDuration = std::chrono::milliseconds;

public:
    EventLoop() {
        auto now = std::chrono::steady_clock::now();  // NOTE: steady_clock: 单调递增稳定时钟
        start_time_ = duration_cast<MSDuration>(now.time_since_epoch());  // 初始化启动时间
    }

    // 返回当前相对启动时间 (毫秒)
    MSDuration time() {
        auto now = std::chrono::steady_clock::now();
        return duration_cast<MSDuration>(now.time_since_epoch()) - start_time_;
    }

    // 延迟一段时间后再调度
    template <typename Rep, typename Period>
    void CallLater(std::chrono::duration<Rep, Period> delay, Handle& callback) {
        CallAt(time() + duration_cast<MSDuration>(delay), callback);
    }

    // 取消调度
    void CancelHandle(Handle& handle) {
        handle.SetState(Handle::UNSCHEDULED);
        cancelled_.insert(handle.GetHandleId());
    }

    // 立即调度 (加入 ready_)
    void CallSoon(Handle& handle) {
        handle.SetState(Handle::SCHEDULED);
        ready_.push({handle.GetHandleId(), &handle});
    }

    struct WaitEventAwaiter {
        bool await_ready() noexcept {
            // 指针自引用检测技巧, 哨兵值技术, 标记特殊状态(没有对应回调, 协程继续执行, 不需要挂起)
            bool ready = (event_.handle_info.handle == (Handle const*)&event_.handle_info.handle);
            event_.handle_info.handle = nullptr;
            return ready;
        }

        template <typename Promise>
        constexpr void await_suspend(std::coroutine_handle<Promise> handle) noexcept {
            handle.promise().SetState(Handle::SUSPEND);  // 设置挂起状态
            event_.handle_info = {.id = handle.promise().GetHandleId(),
                                  // NOTE: 协程的 Promise 对象也是 Handle,
                                  // 因此这里将其注册为事件回调
                                  // 在 RunOnce() 中事件发生时会调用 Run() 方法
                                  .handle = &handle.promise()};
            if (!registered_) {
                selector_.RegisterEvent(event_);  // 注册监听事件
                registered_ = true;
            }
        }

        void await_resume() noexcept {
            event_.handle_info = {};  //< reset callback
        }

        // 移除注册事件
        void destroy() noexcept {
            if (registered_) {
                selector_.RemoveEvent(event_);
                registered_ = false;
            }
        }

        ~WaitEventAwaiter() { destroy(); }

        Selector& selector_;
        Event event_{};
        bool registered_{false};
    };

    // 等待特定的 IO 事件
    [[nodiscard]]
    auto WaitEvent(const Event& event) {
        return WaitEventAwaiter{selector_, event};
    }

    // 运行事件循环直到所有任务完成
    void RunUntilComplete();

private:
    // 判断事件循环是否停止
    bool IsStop() { return schedule_.empty() && ready_.empty() && selector_.IsStop(); }

    // 清理已取消的定时任务
    void CleanupDelayedCall();

    // 在指定时间点执行任务, 加入定时任务堆
    // when: 希望回调被调度的相对时间
    // callback: 回调
    template <typename Rep, typename Period>
    void CallAt(std::chrono::duration<Rep, Period> when, Handle& callback) {
        callback.SetState(Handle::SCHEDULED);  // 设置被调度状态
        // 加入定时任务队
        schedule_.emplace_back(duration_cast<MSDuration>(when),
                               HandleInfo{callback.GetHandleId(), &callback});
        // 保证最小堆的堆顶是最早到期()的任务
        std::ranges::push_heap(schedule_, std::ranges::greater{}, &TimerHandle::first);
    }

    // 执行事件循环的一次迭代
    void RunOnce();

private:
    using TimerHandle = std::pair<MSDuration, HandleInfo>;  // <过期时间, 回调信息>
    MSDuration start_time_;                   // 事件循环启动时间? 咋是 duration 而不是 time_point?
    Selector selector_;                       // 事件选择器 epoll poller
    std::queue<HandleInfo> ready_;            // 就绪队列, 存放已准备好可以立即执行的回调 (Handle)
    std::vector<TimerHandle> schedule_;       // 最小堆, 按 pair 中的时间排序, 管理所有定时任务
    std::unordered_set<HandleId> cancelled_;  // 被取消的回调的 ID (判断是否被取消, 避免错误执行)
};

// 获取 EventLoop (线程安全单例)
EventLoop& GetEventLoop();

}  // namespace cutecoro
