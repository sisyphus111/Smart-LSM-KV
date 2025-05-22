#pragma once
#include "util.h"
#include <iostream>
#include <fstream>
#include <sstream>
repoUtil::repoUtil() {
    std::cout << "util类开始读取数据" << std::endl;

    std::string strPath = "100k_data_ref/cleaned_text_100k.txt";
    std::string vecPath = "100k_data_ref/embedding_100k.txt";
    // 打开文件进行读取
    std::ifstream strFile(strPath);
    std::ifstream vecFile(vecPath);
    if (!strFile.is_open() || !vecFile.is_open()) {
        std::cerr << "Error opening file" << std::endl;
        return;
    }

    std::string line;
    while (std::getline(strFile, line)) {
        if (line == ""){
            std::getline(vecFile, line);
            continue;
        }
        else{
            strs.push_back(line);
            std::getline(vecFile, line);
            // 移除首尾的[ ]
            line = line.substr(1, line.size() - 2);

            std::vector<float> vec;
            vec.reserve(768); // 预分配空间提高性能

            std::istringstream ss(line);
            std::string token;

            // 按逗号分割并转换为float
            while (std::getline(ss, token, ',')) {
                // 去除可能的空格
                token.erase(0, token.find_first_not_of(" \t"));
                token.erase(token.find_last_not_of(" \t") + 1);
                vec.push_back(std::stof(token));
            }

            // 存储向量和对应的字符串
            map[strs[strs.size() - 1]] = std::move(vec);
        }
    }
    // 关闭文件
    strFile.close();
    vecFile.close();
    std::cout << "存储了" << strs.size() << "个字符串和对应的向量" << std::endl;
}

std::string repoUtil::getStr(int index) {
    if (index < 0 || index >= strs.size()) {
        std::cerr << "索引越界" << std::endl;
        return "";
    }
    return strs[index];
}

std::vector<float> repoUtil::getVec(int index) {
    if (index < 0 || index >= strs.size()) {
        std::cerr << "索引越界" << std::endl;
        return {};
    }
    return map[strs[index]];
}

std::vector<float> repoUtil::getVec(std::string str) {
    if (map.find(str) == map.end()) {
        std::cerr << "字符串不存在" << std::endl;
        return {};
    }
    return map[str];
}

repoUtil util;