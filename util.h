#pragma once
#include <vector>
#include <string>
#include <unordered_map>



class repoUtil {
private:
    std::unordered_map<std::string, std::vector<float>> map;
    std::vector<std::string> strs;
public:
    std::string getStr(int index);
    std::vector<float> getVec(int index);
    std::vector<float> getVec(std::string str);

    repoUtil();
};

extern repoUtil util;



