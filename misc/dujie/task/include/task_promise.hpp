#pragma once

#include <condition_variable>
#include <coroutine>
#include <exception>
#include <functional>
#include <mutex>
#include <optional>
#include <vector>

#include "task_forward.hpp"

template <typename ResultType>
struct Task;

template <>
struct Task<void>;

// TODO: 还是要给 Result 封装一下, 外部不知道是 variant

// TODO:
// TaskPromise 对应的 Task
// co_await 对应的 Task

template <typename ResultType>
struct TaskPromise {
public:
    using ResultCallback = std::function<void(Result<ResultType>)>;
    using handle_type = std::coroutine_handle<TaskPromise>;
    auto get_return_object() { return Task{handle_type::from_promise(*this)}; }  // NOTE: CTAD

    std::suspend_never initial_suspend() { return {}; }

    std::suspend_always final_suspend() noexcept { return {}; }

    // 异常处理
    void unhandled_exception() {
        {
            std::unique_lock lk{mtx_};
            result_ = Result<ResultType>{std::current_exception()};
        }
        cv_.notify_all();  // TODO: all 还是 one
        NotifyCallbacks();
    }

    // co_return 返回值
    void return_value(ResultType value) {
        std::unique_lock lk{mtx_};
        result_ = Result{std::move(value)};  // NOTE: CTAD
        cv_.notify_all();                    // TODO: all 还是 one
        NotifyCallbacks();
    }

    // 支持 co_await
    template <typename _ResultType>
    auto await_transform(Task<_ResultType>&& task) {
        return TaskAwaiter{std::forward<Task<_ResultType>>(task)};
    }

public:
    // 获取结果(值/抛出异常)
    ResultType GetResult() {
        std::unique_lock lk{mtx_};
        cv_.wait(lk, [this] { return result_.has_value(); });  // 等待 optional 对象 result_ 有值
        return result_->GetOrThrow();                          // (*result).GetOrThrow()
    }

    void OnCompleted(ResultCallback&& func) {
        std::unique_lock lk{mtx_};
        if (result_.has_value()) {
            auto& res{*result_};  // NOTE: res 包含 val / exception
            lk.unlock();
            func(res);  // 解锁再调用 func
        } else {
            callbacks_.push_back(func);  // TODO: 添加回调函数等待调用
        }
    }

private:
    void NotifyCallbacks() {
        auto& res{*result_};  // NOTE: val / exception
        for (auto& cb : callbacks_) {
            cb(res);  // TODO: 为啥调用每一个回调?
        }
        callbacks_.clear();  // TODO: 为啥调用完成后清空?
    }

private:
    std::optional<Result<ResultType>> result_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::vector<ResultCallback> callbacks_;
};

// 针对 void 类型的 TaskPromise 类模板特化: 不需要结果的异步任务
template <>
struct TaskPromise<void> {
public:
    using ResultCallback = std::function<void(Result<void>)>;
    using handle_type = std::coroutine_handle<TaskPromise<void>>;

    auto get_return_object() { return Task<void>{handle_type::from_promise(*this)}; }
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }

    // 异常处理
    void unhandled_exception() {
        {
            std::unique_lock lk{mtx_};
            result_ = Result<void>{std::current_exception()};
        }
        cv_.notify_all();
        NotifyCallbacks();
    }

    // void 类型使用 return_void
    void return_void() {
        {
            std::unique_lock lk{mtx_};
            result_ = Result<void>{};  // 成功状态
        }
        cv_.notify_all();
        NotifyCallbacks();
    }

    // co_await 支持
    template <typename _ResultType>
    auto await_transform(Task<_ResultType>&& task) {
        return TaskAwaiter<_ResultType>{std::forward<Task<_ResultType>>(task)};
    }

    // 获取结果(无返回值，只抛出异常)
    void GetResult() {
        std::unique_lock lk{mtx_};
        cv_.wait(lk, [this] { return result_.has_value(); });
        result_->GetOrThrow();
    }

    void OnCompleted(ResultCallback&& func) {
        std::unique_lock lk{mtx_};
        if (result_.has_value()) {
            auto& res{*result_};
            lk.unlock();
            func(res);
        } else {
            callbacks_.push_back(func);
        }
    }

private:
    void NotifyCallbacks() {
        auto& res{*result_};
        for (auto& cb : callbacks_) {
            cb(res);
        }
        callbacks_.clear();
    }

private:
    std::optional<Result<void>> result_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::vector<ResultCallback> callbacks_;
};
