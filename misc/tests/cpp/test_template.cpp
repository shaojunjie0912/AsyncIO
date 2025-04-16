#include <iostream>

// 模板全特化

// 通用模板
template <typename T>
struct MyStructFull {
    static void print() { std::cout << "Generic template\n"; }
    T value;
};

template <>
struct MyStructFull<void> {
    static void print() { std::cout << "Generic template\n"; }
};

int main() { MyStructFull<void>::print(); }
