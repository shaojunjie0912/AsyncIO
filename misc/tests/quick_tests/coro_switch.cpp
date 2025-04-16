#include <coroutine>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>

// 自定义协程类型，用于存储和管理协程
class Task {
public:
    // 协程Promise类型，管理协程状态
    struct promise_type {
        Task get_return_object() { return Task{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };

    // 构造函数，获取协程句柄
    Task(std::coroutine_handle<promise_type> handle) : handle_(handle) {}

    // 析构函数，清理协程资源
    ~Task() {
        if (handle_ && handle_.done()) {
            handle_.destroy();
        }
    }

    // 禁用拷贝操作
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    // 允许移动操作
    Task(Task&& other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_ && handle_.done()) handle_.destroy();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    // 检查协程是否完成
    bool is_done() const { return handle_.done(); }

    // 恢复协程执行
    void resume() {
        if (handle_ && !handle_.done()) {
            handle_();
        }
    }

    // 获取协程句柄（添加这个方法以便访问内部句柄）
    std::coroutine_handle<promise_type> get_handle() const { return handle_; }

private:
    std::coroutine_handle<promise_type> handle_;
};

// 自定义可等待对象，用于协程之间的切换
class SwitchTo {
public:
    explicit SwitchTo(Task& target) : target_(target) {}

    bool await_ready() { return false; }

    void await_suspend(std::coroutine_handle<> handle) {
        // 保存当前协程句柄，以便目标协程可以返回
        source_ = handle;
        // 恢复目标协程
        target_.resume();
    }

    void await_resume() {}

    // 返回到源协程
    void switch_back() {
        if (source_) {
            source_();
        }
    }

private:
    Task& target_;
    std::coroutine_handle<> source_ = nullptr;
};

// 修改为共享全局对象，避免悬空指针
std::shared_ptr<SwitchTo> switch_to_first;
std::shared_ptr<SwitchTo> switch_to_second;

// 第二个协程
Task second_coroutine() {
    std::cout << "开始执行second_coroutine()" << std::endl;

    std::cout << "second_coroutine()执行一些工作" << std::endl;

    std::cout << "second_coroutine()准备切回first_coroutine()" << std::endl;
    // 切回第一个协程
    switch_to_first->switch_back();

    std::cout << "second_coroutine()恢复执行" << std::endl;

    std::cout << "second_coroutine()结束" << std::endl;
    // 协程结束
    co_return;
}

// 第一个协程
Task first_coroutine() {
    std::cout << "开始执行first_coroutine()" << std::endl;

    std::cout << "first_coroutine()准备切换到second_coroutine()" << std::endl;
    // 切换到第二个协程，并挂起当前协程
    co_await *switch_to_second;

    std::cout << "first_coroutine()恢复执行" << std::endl;

    std::cout << "first_coroutine()再次切换到second_coroutine()" << std::endl;
    // 再次切换到第二个协程
    co_await *switch_to_second;

    std::cout << "first_coroutine()完成" << std::endl;
    // 协程结束
    co_return;
}

// 主函数
int main() {
    std::cout << "程序开始执行" << std::endl;

    // 创建第二个协程
    Task second = second_coroutine();

    // 创建第一个协程
    Task first = first_coroutine();

    // 初始化协程切换器
    switch_to_second = std::make_shared<SwitchTo>(second);
    switch_to_first = std::make_shared<SwitchTo>(first);

    std::cout << "主函数等待所有协程完成" << std::endl;

    // 手动启动第一个协程，开始协程链
    first.resume();

    // 确保恢复第二个协程以完成执行
    if (!second.is_done()) {
        second.resume();
    }

    std::cout << "程序执行结束" << std::endl;
    return 0;
}
