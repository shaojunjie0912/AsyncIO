#include <iostream>
#include <vector>

using namespace std;

int main() {
    std::vector<int> v{1, 2};
    for (int i = 0; i < v.size(); ++i) {
        v.push_back(1);
    }
    cout << "dsad" << endl;
}