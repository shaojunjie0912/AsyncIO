#pragma once

#include <coroutine>
#include <functional>

#include "task_forward.hpp"

template <typename ResultType>
struct Task {
    using ResultCallback = std::function<void(Result<ResultType>)>;
    using ExceptionCallback = std::function<void(std::exception const &)>;
    using promise_type = TaskPromise<ResultType>;

    ResultType GetResult();

    Task &Then(ResultCallback &&func);
    Task &Catching(ExceptionCallback &&func);
    Task &Finally(std::function<void()> &&func);

    explicit Task(std::coroutine_handle<promise_type> handle) noexcept : h_(handle) {}

    ~Task() noexcept;

    // move-only
    Task(Task &&other) noexcept;
    Task &operator=(Task &&other) noexcept;
    Task(Task const &) = delete;
    Task &operator=(Task const &) = delete;

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

    explicit Task(std::coroutine_handle<promise_type> handle) noexcept : h_(handle) {}

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
