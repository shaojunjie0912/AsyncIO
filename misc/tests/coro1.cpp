#include <coroutine>
#include <iostream>

struct Task {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

public:
    explicit Task(handle_type h) : h_(h) {}
    ~Task() { Destory(); }

    // move-only
    Task(Task const&) = delete;
    Task& operator=(Task const&) = delete;

    Task(Task&& other) noexcept {
        h_ = other.h_;
        other.h_ = {};
    }

    Task& operator=(Task&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        Destory();
        h_ = other.h_;
        other.h_ = {};
        return *this;
    }

public:
    struct promise_type {
        Task get_return_object() { return Task{handle_type::from_promise(*this)}; }

        std::suspend_always initial_suspend() { return {}; }

        std::suspend_never final_suspend() noexcept { return {}; }

        void return_void() {}

        void unhandled_exception() {}
    };

public:
    void Resume() { h_.resume(); }
    void Destory() {
        if (h_) {
            h_.destroy();
        }
    }

private:
    handle_type h_;
};

Task my_coroutine() { co_return; }

int main() {
    auto t = my_coroutine();
    t.Resume();
    return 0;
}