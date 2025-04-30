#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <system_error>

#include "co_async/and_then.hpp"
#include "co_async/debug.hpp"
#include "co_async/task.hpp"
#include "co_async/timer_loop.hpp"
#include "co_async/when_all.hpp"
#include "co_async/when_any.hpp"

namespace co_async {

auto checkError(auto res) {
    // 把C语言错误码转换为C++异常
    if (res == -1) [[unlikely]] {
        throw std::system_error(errno, std::system_category());
    }
    return res;
}

struct EpollFilePromise : Promise<void> {
    auto get_return_object() {
        return std::coroutine_handle<EpollFilePromise>::from_promise(*this);
    }

    EpollFilePromise &operator=(EpollFilePromise &&) = delete;

    int mFileNo;       // 文件描述符
    uint32_t mEvents;  // 监听的事件
};

struct EpollLoop {
    void addListener(EpollFilePromise &promise) {
        struct epoll_event event;
        event.events = promise.mEvents;
        event.data.ptr = &promise;  // 把 EpollFilePromise 存进 event.data.ptr, 后面可以转回来
        checkError(epoll_ctl(mEpoll, EPOLL_CTL_ADD, promise.mFileNo, &event));
    }

    void tryRun() {
        struct epoll_event ebuf[10];
        int res = checkError(epoll_wait(mEpoll, ebuf, 10, -1));
        for (int i = 0; i < res; i++) {
            auto &event = ebuf[i];
            auto &promise = *(EpollFilePromise *)event.data.ptr;  // 拿到 event.data.ptr 转回来
            checkError(epoll_ctl(mEpoll, EPOLL_CTL_DEL, promise.mFileNo, NULL));
            // 获取 promise 对应协程句柄, 恢复协程
            // NOTE: 咋还能直接调用这个函数..
            std::coroutine_handle<EpollFilePromise>::from_promise(promise).resume();
        }
    }

    EpollLoop &operator=(EpollLoop &&) = delete;
    ~EpollLoop() { close(mEpoll); }  // EpollLoop 析构时关闭 epoll_fd

    int mEpoll = checkError(epoll_create1(0));
};

struct EpollFileAwaiter {
    bool await_ready() const noexcept { return false; }

    // 也就是如果外部 co_await 一下, 这里就会把文件描述符和监听事件加入 epoll 事件循环
    void await_suspend(std::coroutine_handle<EpollFilePromise> coroutine) const {
        auto &promise = coroutine.promise();
        promise.mFileNo = mFileNo;
        promise.mEvents = mEvents;
        loop.addListener(promise);
    }

    void await_resume() const noexcept {}

    using ClockType = std::chrono::system_clock;

    EpollLoop &loop;  // 事件循环
    int mFileNo;
    uint32_t mEvents;
};

// 封装了一个协程
// 第一个模板参数代表协程返回类型: void 就是无返回值
// 第二个模板参数代表自定义 Promise 类型
inline Task<void, EpollFilePromise> wait_file(EpollLoop &loop, int fileNo, uint32_t events) {
    co_await EpollFileAwaiter(loop, fileNo, events);
}

}  // namespace co_async

co_async::EpollLoop loop;

co_async::Task<std::string> reader() {
    co_await wait_file(loop, 0, EPOLLIN);  // 0 是 stdin 标准输入文件描述符
    std::string s;
    while (true) {
        char c;
        ssize_t len = read(0, &c, 1);  // 每次读一个?
        if (len == -1) {               // 由于是非阻塞, 那么读不到数据就 break 掉吧
            if (errno != EWOULDBLOCK) [[unlikely]] {
                throw std::system_error(errno, std::system_category());
            }
            break;
        }
        s.push_back(c);
    }
    co_return s;
}

co_async::Task<void> async_main() {
    while (true) {
        auto s = co_await reader();
        debug(), "读到了", s;
        if (s == "quit\n") break;
    }
}

// NOTE: 真优雅!

int main() {
    int attr = 1;
    ioctl(0, FIONBIO, &attr);  // 设置文件描述符非阻塞

    auto t = async_main();
    t.mCoroutine.resume();
    while (!t.mCoroutine.done()) {
        loop.tryRun();
    }

    return 0;
}
