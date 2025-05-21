#pragma once
#include <vector>
#include <string>
#include <unordered_map>


//  ref文件的格式就是一行有内容一行空？

class repoUtil {
private:
    std::unordered_map<std::string, std::vector<float>> map;
    std::vector<std::string> strs;
public:
    void init();
    std::string getStr(int index);
    std::vector<float> getVec(int index);
    std::vector<float> getVec(std::string str);
};



