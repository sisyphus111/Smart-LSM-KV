#pragma once
#include <vector>
#include <cstdint>
#include <random>
#include "embedding.h"

// 注：这个HNSWIndex似乎不支持删除
// 另外，插入键相同的键值对的规定行为是覆盖，但这里不支持
class HNSWIndex {
public:
    void insert(const std::vector<float>& embedding, uint64_t key); // 插入
    std::vector<uint64_t> search_knn_hnsw(const std::vector<float>& query, int k); // 搜索k个最近邻，按照相似度降序返回key的向量

    HNSWIndex();
    ~HNSWIndex();
private:
    std::random_device rd;
    std::mt19937 gen;


    int M = 6; // 插入过程中，被插入节点需要与图中其他节点建立的连接数
    int M_max = 10; // 连接数的上限，若超过则需要删除部分连接
    int efConstruction = 50; // 搜索过程中候选节点集合的数量
    int m_L = 6; // 节点的最高层数

    double grow = 0.5; // 节点的增长率


    struct Node {
        std::vector<float> embedding; // 节点的向量

        std::vector<std::vector<Node *>> neighbors; // 每层的邻居节点们

        uint64_t key;
        int level; // 节点的层数

        int m_L = 6; // 节点的最高层数

        Node(int level, uint64_t key, const std::vector<float>& embedding): level(level), key(key), embedding(embedding) {
            neighbors.resize(m_L);
        }
    };

    Node *entry; // 查找、插入的入口节点
    int getRandomLevel(); // 获取随机层数


};
