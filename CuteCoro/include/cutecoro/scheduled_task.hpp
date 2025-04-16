#pragma once

#include <cutecoro/concepts/future.hpp>
#include <cutecoro/noncopyable.hpp>

namespace cutecoro {

// Task 包装类
template <concepts::Future Task>
struct ScheduledTask : NonCopyable {
    // 构造函数, 就是将 Task 加入调度
    template <concepts::Future Fut>
    explicit ScheduledTask(Fut&& fut) : task_(std::forward<Fut>(fut)) {
        if (task_.valid() && !task_.done()) {
            task_.handle_.promise().schedule();
        }
    }

public:
    // 取消任务
    void cancel() { task_.destroy(); }

    decltype(auto) operator co_await() const& noexcept { return task_.operator co_await(); }

    decltype(auto) operator co_await() const&& noexcept { return task_.operator co_await(); }

    // 左值调用 get_result
    decltype(auto) get_result() & { return task_.get_result(); }

    // 右值调用 get_result
    decltype(auto) get_result() && { return std::move(task_).get_result(); }

    // 判断任务是否有效
    bool valid() const { return task_.valid(); }

    // 判断任务是否完成
    bool done() const { return task_.done(); }

private:
    Task task_;
};

// CTAD guide
template <concepts::Future Fut>
ScheduledTask(Fut&&) -> ScheduledTask<Fut>;

template <concepts::Future Fut>
[[nodiscard("discard(detached) a task will not schedule to run")]]
ScheduledTask<Fut> schedule_task(Fut&& fut) {
    return ScheduledTask{std::forward<Fut>(fut)};  // CTAD
}

}  // namespace cutecoro
