#pragma once

#include <coroutine>
#include <functional>

template <typename ResultType>
struct Task {
    using ResultCallback = std::function<void(Result<ResultType>)>;
    using ExceptionCallback = std::function<void(std::exception const &)>;
    using promise_type = TaskPromise<ResultType>;

    ResultType GetResult() { return h_.promise().GetResult(); }

    Task &Then(ResultCallback &&func) {
        h_.promise().OnCompleted([func](auto result) {  // NOTE: auto -> 模板 lambda
            try {
                func(result.GetOrThrow());
            } catch (std::exception const &e) {
                // TODO: ignore.
            }
        });
        return *this;
    }

    Task &Catching(ExceptionCallback &&func) {
        h_.promise().OnCompleted([func](auto result) {
            try {
                result.GetOrThrow();
            } catch (std::exception const &e) {
                func(e);
            }
        });
        return *this;
    }

    Task &Finally(std::function<void()> &&func) {
        h_.promise().OnCompleted([func](auto result) { func(); });  // TODO: result 不用?
        return *this;
    }

    explicit Task(std::coroutine_handle<promise_type> handle) noexcept : h_(handle) {}

    ~Task() noexcept {
        if (h_) {
            h_.destroy();
        }
    }

    // move-only
    Task(Task &&other) noexcept : h_(std::exchange(other.h_, {})) {}
    Task &operator=(Task &&other) noexcept {
        if (this != &other) {
            if (h_) {
                h_.destroy();
            }
            h_ = std::exchange(other.h_, {});
        }
        return *this;
    }
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

    void GetResult() { return h_.promise().GetResult(); }

    Task &Then(ResultCallback &&func) {
        h_.promise().OnCompleted([func](auto result) {
            try {
                result.GetOrThrow();
                func();
            } catch (std::exception const &) {
                // 忽略异常
            }
        });
        return *this;
    }

    Task &Catching(ExceptionCallback &&func) {
        h_.promise().OnCompleted([func](auto result) {
            try {
                result.GetOrThrow();
            } catch (std::exception const &e) {
                func(e);
            }
        });
        return *this;
    }

    Task &Finally(std::function<void()> &&func) {
        h_.promise().OnCompleted([func](auto) { func(); });
        return *this;
    }

    explicit Task(std::coroutine_handle<promise_type> handle) noexcept : h_(handle) {}

    ~Task() noexcept {
        if (h_) {
            h_.destroy();
        }
    }

    // move-only
    Task(Task &&other) noexcept : h_(std::exchange(other.h_, {})) {}
    Task &operator=(Task &&other) noexcept {
        if (this != &other) {
            if (h_) {
                h_.destroy();
            }
            h_ = std::exchange(other.h_, {});
        }
        return *this;
    }
    Task(Task const &) = delete;
    Task &operator=(Task const &) = delete;

private:
    std::coroutine_handle<promise_type> h_;
};
