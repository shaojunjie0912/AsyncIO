#include <iostream>

void Print(auto const&... args) {
    ((std::cout << args << " "), ...);
    std::cout << std::endl;
}

int main() { Print(1, 2, 3, 4); }