#include <chrono>
#include <task/task.hpp>
#include <task/utils/io_utils.hpp>
#include <thread>

using namespace std::chrono_literals;

// NOTE: 线程阻塞 + 协程切换

Task<int> simple_task2() {
    debug("task 2 start ...");
    std::this_thread::sleep_for(1s);
    debug("task 2 returns after 1s.");
    co_return 2;
}

Task<int> simple_task3() {
    debug("in task 3 start ...");
    std::this_thread::sleep_for(2s);
    debug("task 3 returns after 2s.");
    co_return 3;
}

Task<int> simple_task() {
    debug("task start ...");
    auto result2 = co_await simple_task2();  // NOTE: 挂起当前协程 simple_task 让出执行权给被等待的任务 simple_task2
    debug("returns from task2: ", result2);
    auto result3 = co_await simple_task3();
    debug("returns from task3: ", result3);
    co_return 1 + result2 + result3;
}

int main() {
    auto simpleTask = simple_task();

    // 异步
    simpleTask.Then([](int i) { debug("simple task end: ", i); }).Catching([](std::exception const &e) {
        debug("error occurred", e.what());
    });

    // 同步
    try {
        auto i = simpleTask.GetResult();
        debug("simple task end from get: ", i);
    } catch (std::exception &e) {
        debug("error: ", e.what());
    }
    return 0;
}
