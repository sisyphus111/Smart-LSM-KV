#include "../kvstore.h"
#include <fstream>
#include <vector>
#include <iostream>

std::vector<std::string> load_text(std::string filename) {
    std::ifstream file(filename);
    std::string line;
    std::vector<std::string> text;
    while (std::getline(file, line)) {
        text.push_back(line);
    }
    return text;
}

int main() {
    KVStore store("data/");
    store.reset();

    std::string testStr = std::string(1024*64 - 1, 's');
    std::vector<float> testVec = embedding(testStr)[0];

    std::cout << testVec.size() << std::endl;

    return 0;
}

