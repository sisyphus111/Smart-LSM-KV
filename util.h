#pragma once
#include <vector>
#include <string>
#include <unordered_map>
class util {
private:
    std::unordered_map<std::string, std::vector<float>> map;
    std::vector<std::string> strs;
public:
    void init();
    std::string getStr(int index);
    std::vector<float> getVec(int index);
    std::vector<float> getVec(std::string str);
};



