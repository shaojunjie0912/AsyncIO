#include <fmt/core.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <vector>

int main() {
    std::vector<int> v{3, 1, 4, 1, 5, 9};
    fmt::println("v: {}", v);
    std::make_heap(v.begin(), v.end());
    fmt::println("v: {}", v);
    v.push_back(6);
    fmt::println("v: {}", v);
    std::push_heap(v.begin(), v.end());
    fmt::println("v: {}", v);
}