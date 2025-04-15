#pragma once

#include <coroutine>

#include "executor.hpp"

struct DispatchAwaiter {
public:
    explicit DispatchAwaiter(AbstractExecutor* executor) : executor_(executor) {}

public:
    bool await_ready() { return false; }

    void await_suspend(std::coroutine_handle<> handle) {
        executor_->Execute([handle] { handle.resume(); });
    }

    void await_resume() {}

private:
    AbstractExecutor* executor_;
};
