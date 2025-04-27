#include <embedding.h>
#include <iostream>
#include <string>
#include <vector>

int main() {
  std::string text = "Hello, World!";
  auto res = embedding(text);
  for (auto &vec : res) {
    for (auto &val : vec) {
      std::cout << val << "\n";
    }
    std::cout << std::endl;
  }
  return 0;
}