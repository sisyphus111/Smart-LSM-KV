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

    std::vector<std::string> text(3);
    text[0] = "Hello, world!";
    text[1] = "This is a test.";
    text[2] = "Goodbye, world!";

    store.put(0, text[0]);
    std::cout << "Inserted: " << text[0] << std::endl;
    std::cout << "getkey(0):" << store.get(0) << std::endl;
    store.put(0, text[0]);
    std::cout << "Reinserted: " << text[0] << std::endl;
    std::cout << "getkey(0):" << store.get(0) << std::endl;
    bool equal = (store.get(0) == text[0]);
    std::cout << "getkey(0) == text[0]: " << equal << std::endl;
    return 0;
}

