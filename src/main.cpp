#include <iostream>

namespace tests {

void TestAll();

} // namespace tests

int main() {
    try {
        tests::TestAll();
        std::system("pause");
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}
