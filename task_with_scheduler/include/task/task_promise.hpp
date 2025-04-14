#pragma once

#include <condition_variable>
#include <coroutine>
#include <exception>
#include <functional>
#include <mutex>
#include <optional>
#include <vector>

#include "task_forward.hpp"
//
#include "dispatch_awaiter.hpp"
#include "task_awaiter.hpp"
//
#include "result.hpp"

// TODO: 还是要给 Result 封装一下, 外部不知道是 variant

// TODO:
// TaskPromise 对应的 Task
// co_await 对应的 Task

template <typename ResultType, typename Executor>
struct TaskPromise {
public:
    using ResultCallback = std::function<void(Result<ResultType>)>;
    using handle_type = std::coroutine_handle<TaskPromise>;

    auto get_return_object();

    DispatchAwaiter initial_suspend();

    // 协程结束时挂起
    std::suspend_always final_suspend() noexcept;

    void unhandled_exception();

    void return_value(ResultType value);

    // TODO: 类模板 TaskPormise 中的函数模板 await_transform 的模板参数
    // 要支持跟类模板 Task 不同的类型, 允许一个协程内部可以 co_await 不同类型的 Task
    template <typename OtherResultType, typename OtherExecutor>
    TaskAwaiter<OtherResultType, OtherExecutor> await_transform(Task<OtherResultType, OtherExecutor>&& task);

public:
    // 阻塞获取结果值(但其实如果任务已经完成, 肯定是有值的, 那么就不会阻塞)
    ResultType GetResult();

    // 用于注册一个在任务完成(有结果或抛出异常)后执行的回调函数
    // 如果 result_ 已有值 (任务已完成), 则立刻调用该回调;
    // 否则将回调存入 callbacks_, 待任务完成后再统一执行
    void OnCompleted(ResultCallback&& func);

private:
    void NotifyCallbacks();

private:
    Executor executor_;
    std::optional<Result<ResultType>> result_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::vector<ResultCallback> callbacks_;
};

#include "task_promise_impl.hpp"
