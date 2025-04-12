#include <iostream>
#include <memory>
#include <utility>

using namespace std;

struct MoveOnly {
    int* p_;

    MoveOnly(int val) : p_(new int{val}) {}

    ~MoveOnly() noexcept {
        if (p_) {  // 这个检查很关键
            delete p_;
            p_ = nullptr;
        }
    }

    MoveOnly(MoveOnly const&) = delete;
    MoveOnly& operator=(MoveOnly const&) = delete;

    MoveOnly(MoveOnly&& other) noexcept : p_(std::exchange(other.p_, nullptr)) {}

    MoveOnly& operator=(MoveOnly&& other) noexcept {
        if (this != &other) {
            if (p_) {
                delete p_;
            }
            p_ = std::exchange(other.p_, nullptr);
        }
        return *this;
    }

    MoveOnly Trans() { return std::move(*this); }

    void Print() { std::cout << "val: " << *p_ << '\n'; }
};

template <typename T>
struct Foo {};

template <typename T>
struct Bar {
    Bar(Foo<T>&& foo) : foo_(std::forward<Foo<T>>(foo)) {}
    Foo<T> foo_;
};

int main() {
    auto t{MoveOnly{4}.Trans()};
    t.Print();
    return 0;
}