#pragma once

#include <coroutine>
#include <functional>

#include "task_forward.hpp"

template <typename ResultType>
struct Task {
    using ResultValueCallback = std::function<void(ResultType)>;
    using ResultExceptionCallback = std::function<void(std::exception const &)>;
    using promise_type = TaskPromise<ResultType>;

    explicit Task(std::coroutine_handle<promise_type> handle);

    ~Task() noexcept;

    // move-only
    Task(Task &&other) noexcept;

    Task &operator=(Task &&other) noexcept;

    Task(Task const &) = delete;

    Task &operator=(Task const &) = delete;

public:
    // 阻塞获取结果值 (也见 PromiseType::GetResult())
    ResultType GetResult();

    // 在任务成功完成并产生结果时执行一个回调 (可拿到协程返回值)
    // 返回 Task& 为了链式调用
    Task &Then(ResultValueCallback &&func);

    // 在任务执行过程中出现异常时执行一个回调 (可捕获并处理抛出的异常)
    // 返回 Task& 为了链式调用
    Task &Catching(ResultExceptionCallback &&func);

    Task &Finally(std::function<void()> &&func);

private:
    std::coroutine_handle<promise_type> h_;
};

// 针对 void 类型的 Task 类模板特化: 不需要结果的异步任务
template <>
struct Task<void> {
    using ResultCallback = std::function<void()>;
    using ExceptionCallback = std::function<void(std::exception const &)>;
    using promise_type = TaskPromise<void>;

    void GetResult();

    Task &Then(ResultCallback &&func);
    Task &Catching(ExceptionCallback &&func);
    Task &Finally(std::function<void()> &&func);

    explicit Task(std::coroutine_handle<promise_type> handle);

    ~Task() noexcept;

    // move-only
    Task(Task &&other) noexcept;
    Task &operator=(Task &&other) noexcept;
    Task(Task const &) = delete;
    Task &operator=(Task const &) = delete;

private:
    std::coroutine_handle<promise_type> h_;
};

#include "task.inl"
