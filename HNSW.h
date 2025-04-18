#pragma once
#include <vector>
#include <cstdint>
#include <random>


class HNSWIndex {
public:
    void insert(const std::vector<float>& embedding, uint64_t key); // 插入
    std::vector<uint64_t> search_knn_hnsw(const std::vector<float>& query, int k); // 搜索k个最近邻，按照相似度降序返回key的向量

    HNSWIndex();
private:
    std::random_device rd;
    std::mt19937 gen;


    int M = 6; // 插入过程中，被插入节点需要与图中其他节点建立的连接数
    int M_max = 8; // 连接数的上限，若超过则需要删除部分连接
    int efConstruction = 30; // 搜索过程中候选节点集合的数量
    int m_L = 6; // 节点的最高层数

    struct Node {
        std::vector<float> embedding; // 节点的向量
        std::vector<std::vector<Node *>> neighbors; // 每层的邻居节点们
        int level; // 节点的层数
        std::vector<int> neighbor_count; // 每层的邻居数
    };

    Node *head; // 头节点
    int getRandomLevel(); // 获取随机层数
};
