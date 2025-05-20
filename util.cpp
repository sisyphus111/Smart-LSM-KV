#pragma once
#include "util.h"
#include <iostream>
#include <fstream>
#include <sstream>
void util::init() {
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
    // 读取字符串文件
    while (std::getline(strFile, line)) {
        strs.push_back(line);
    }
    strFile.close();
    std::cout << "成功加载 " << strs.size() << " 个字符串" << std::endl;



    // 读取向量文件
    int lineCount = 0;
    while (std::getline(vecFile, line)) {
        // 检查行格式
        if (line.size() < 2 || line.front() != '[' || line.back() != ']') {
            std::cerr << "无效向量格式，行号: " << lineCount << std::endl;
            continue;
        }

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

            try {
                vec.push_back(std::stof(token));
            } catch (const std::exception& e) {
                std::cerr << "浮点数转换错误: " << token << std::endl;
            }
        }

        // 检查向量维度
        if (vec.size() != 768) {
            std::cerr << "警告：第" << lineCount << "行向量维度为" << vec.size() << "，应为768" << std::endl;
        }

        // 存储向量和对应的字符串
        if (lineCount < strs.size()) {
            map[strs[lineCount]] = std::move(vec);
        } else {
            std::cerr << "警告：向量数量超过字符串数量，行号: " << lineCount << std::endl;
            break;
        }

        lineCount++;
    }
    vecFile.close();

    std::cout << "成功加载 " << lineCount << " 个向量" << std::endl;

}

std::string util::getStr(int index) {
    if (index < 0 || index >= strs.size()) {
        std::cerr << "索引越界" << std::endl;
        return "";
    }
    return strs[index];
}

std::vector<float> util::getVec(int index) {
    if (index < 0 || index >= strs.size()) {
        std::cerr << "索引越界" << std::endl;
        return {};
    }
    return map[strs[index]];
}

std::vector<float> util::getVec(std::string str) {
    if (map.find(str) == map.end()) {
        std::cerr << "字符串不存在" << std::endl;
        return {};
    }
    return map[str];
}