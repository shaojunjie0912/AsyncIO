#pragma once

#include "task/task_awaiter.hpp"
#include "task_promise.hpp"
//
#include "task.hpp"

// NOTE: 为什么模板类型推断失败
// template <typename ResultType>
// template <typename _ResultType>
// auto TaskPromise<ResultType>::await_transform(Task<_ResultType>&& task) {
//     return TaskAwaiter{std::move(task)};
// }
// NOTE: 因为 TaskAwaiter 构造函数参数为 Task<ResultType> &&
// 编译器不知道用哪个类型实例化模板参数

template <typename ResultType>
auto TaskPromise<ResultType>::get_return_object() {
    return Task{handle_type::from_promise(*this)};
}

template <typename ResultType>
std::suspend_never TaskPromise<ResultType>::initial_suspend() {
    return {};
}

template <typename ResultType>
std::suspend_always TaskPromise<ResultType>::final_suspend() noexcept {
    return {};
}

template <typename ResultType>
void TaskPromise<ResultType>::unhandled_exception() {
    {
        std::unique_lock lk{mtx_};
        result_ = Result<ResultType>{std::current_exception()};
    }
    cv_.notify_all();
    NotifyCallbacks();
}

template <typename ResultType>
void TaskPromise<ResultType>::return_value(ResultType value) {
    std::unique_lock lk{mtx_};
    result_ = Result{std::move(value)};  // 存储结果值 value 到 Result 对象中(NOTE: 标志当前任务完成)
    cv_.notify_all();                    // 通知所有正在等待的线程(NOTE: 当前只是单线程, 等多线程再看)
    NotifyCallbacks();                   // 执行所有回调函数(即之前注册的恢复 simple_task 的回调)
}

template <typename ResultType>
template <typename OtherResultType>
TaskAwaiter<OtherResultType> TaskPromise<ResultType>::await_transform(Task<OtherResultType>&& task) {
    return TaskAwaiter<OtherResultType>{std::move(task)};
}

template <typename ResultType>
ResultType TaskPromise<ResultType>::GetResult() {
    std::unique_lock lk{mtx_};
    cv_.wait(lk, [this] { return result_.has_value(); });
    return result_->GetOrThrow();
}

template <typename ResultType>
void TaskPromise<ResultType>::OnCompleted(ResultCallback&& func) {
    std::unique_lock lk{mtx_};
    if (result_.has_value()) {  // 如果 result_ 有值, 则解锁后调用 func
        auto& res{*result_};
        lk.unlock();
        func(res);
    } else {  // 否则注册回调函数([](){恢复 simple_task} 的回调就是在这里加入 simple_task2 的 TaskPromise 中的)
        callbacks_.push_back(std::move(func));
    }
}

template <typename ResultType>
void TaskPromise<ResultType>::NotifyCallbacks() {
    auto& res{*result_};
    for (auto& cb : callbacks_) {
        cb(res);
    }
    callbacks_.clear();
}

// void specialization implementations
auto TaskPromise<void>::get_return_object() { return Task<void>{handle_type::from_promise(*this)}; }

std::suspend_never TaskPromise<void>::initial_suspend() { return {}; }

std::suspend_always TaskPromise<void>::final_suspend() noexcept { return {}; }

void TaskPromise<void>::unhandled_exception() {
    {
        std::unique_lock lk{mtx_};
        result_ = Result<void>{std::current_exception()};
    }
    cv_.notify_all();
    NotifyCallbacks();
}

void TaskPromise<void>::return_void() {
    {
        std::unique_lock lk{mtx_};
        result_ = Result<void>{};
    }
    cv_.notify_all();
    NotifyCallbacks();
}

template <typename OtherResultType>
TaskAwaiter<OtherResultType> TaskPromise<void>::await_transform(Task<OtherResultType>&& task) {
    return TaskAwaiter<OtherResultType>{std::move(task)};
}

void TaskPromise<void>::GetResult() {
    std::unique_lock lk{mtx_};
    cv_.wait(lk, [this] { return result_.has_value(); });
    result_->GetOrThrow();
}

void TaskPromise<void>::OnCompleted(ResultCallback&& func) {
    std::unique_lock lk{mtx_};
    if (result_.has_value()) {
        auto& res{*result_};
        lk.unlock();
        func(res);
    } else {
        callbacks_.push_back(std::move(func));
    }
}

void TaskPromise<void>::NotifyCallbacks() {
    auto& res{*result_};
    for (auto& cb : callbacks_) {
        cb(res);
    }
    callbacks_.clear();
}
