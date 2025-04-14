#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <unordered_map>

// 调度器抽象基类
class AbstractExecutor {
public:
    virtual ~AbstractExecutor() = default;

    virtual void Execute(std::function<void()> &&func) = 0;
};

// 直接执行
class NoopExecutor : public AbstractExecutor {
public:
    void Execute(std::function<void()> &&func) override { func(); }
};

// 在新线程执行
class NewThreadExecutor : public AbstractExecutor {
public:
    void Execute(std::function<void()> &&func) override { std::thread(func).detach(); }
};

// class AsyncExecutor : public AbstractExecutor {
// public:
//     void Execute(std::function<void()> &&func) override {
//         int id{};
//         {
//             std::unique_lock lk{mtx_};
//             id = nextId++;
//         }

//         auto future = std::async([this, id, func] {
//             func();
//             std::unique_lock lock(mtx_);
//             // move future to stack so that it will be destroyed after unlocked.
//             auto f = std::move(this->futures[id]);
//             this->futures.erase(id);
//         });

//         std::unique_lock lk{mtx_};
//         this->futures[id] = std::move(future);
//     }

// private:
//     std::mutex mtx_;
//     int nextId = 0;
//     std::unordered_map<int, std::future<void>> futures{};
// };