#include <iostream>
#include <string>
#include <variant>

// std::get<type>(variant)
// p = std::get_if<type>(&variant) 不为空则能正常获取

int main() {
    std::variant<int, std::string> value_variant = "das";

    if (std::holds_alternative<int>(value_variant)) {
        std::cout << "Variant holds int." << std::endl;
    } else {
        std::cout << std::get<int>(value_variant);
        std::cout << "Variant does not hold int." << std::endl;
    }

    return 0;
}
