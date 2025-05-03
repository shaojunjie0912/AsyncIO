#pragma once

#include <cutecoro/detail/concepts/future.hpp>
#include <cutecoro/detail/noncopyable.hpp>

namespace cutecoro {

// Task 包装类
template <concepts::Future TaskType>
struct ScheduledTask : NonCopyable {
    // 构造函数, 就是将 Task 加入调度
    template <concepts::Future Fut>
    explicit ScheduledTask(Fut&& task) : task_(std::forward<Fut>(task)) {
        if (task_.IsValid() && !task_.IsDone()) {
            task_.handle_.promise().Schedule();
        }
    }

public:
    decltype(auto) operator co_await() const& noexcept { return task_.operator co_await(); }

    decltype(auto) operator co_await() const&& noexcept { return task_.operator co_await(); }

    // 取消任务
    void Cancel() { task_.Destroy(); }

    // 左值调用 GetResult
    decltype(auto) GetResult() & { return task_.GetResult(); }

    // 右值调用 GetResult
    decltype(auto) GetResult() && { return std::move(task_).GetResult(); }

    // 判断任务是否有效
    bool IsValid() const { return task_.IsValid(); }

    // 判断任务是否完成
    bool IsDone() const { return task_.IsDone(); }

private:
    TaskType task_;
};

// deduction guide 类模板参数推导指引
template <concepts::Future Fut>
ScheduledTask(Fut&&) -> ScheduledTask<Fut>;

template <concepts::Future Fut>
[[nodiscard("忽略 (分离) 一个任务将不会被调度执行")]]
ScheduledTask<Fut> schedule_task(Fut&& fut) {
    return ScheduledTask{std::forward<Fut>(fut)};  // 有了推导指引, 这里的 CTAD 才能成功
}

}  // namespace cutecoro
