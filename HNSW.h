#pragma once
#include <vector>
#include <cstdint>
#include <random>
#include <set>
#include "embedding.h"

#define max_L 6

struct Node {
    uint64_t key; // key
    std::vector<float> embedding; // 嵌入向量

    std::vector<std::vector<Node *>> neighbors; // 每层的邻居节点们

    int level; // 节点的层数

    int m_L = max_L; // 节点的最高层数

    Node(int level, uint64_t key, const std::vector<float>& embedding, uint64_t m_L = max_L): level(level), key(key), embedding(embedding) {
        neighbors.resize(m_L);
    }
};


class HNSWIndex {
public:

    void insert(const std::vector<float>& embedding, uint64_t key); // 插入
    std::vector<uint64_t> search_knn_hnsw(const std::vector<float>& query, int k); // 搜索k个最近邻，按照相似度降序返回key的向量

    void del(uint64_t key, const std::vector<float>& vec); // 删除某个键-嵌入向量对，使用lazy delete

    bool isInDeletedNodes(uint64_t key, const std::vector<float>& query);
    void restoreDeletedNode(uint64_t key, const std::vector<float>& query);

    HNSWIndex();
    HNSWIndex(int M, int M_max, int efConstruction, Node* entry, int m_L);

    void saveToDisk(const std::string &hnsw_data_root); // 保存HNSW索引到磁盘

    ~HNSWIndex();
private:
    std::random_device rd;
    std::mt19937 gen;


    int M = 6; // 插入过程中，被插入节点需要与图中其他节点建立的连接数
    int M_max = 12; // 连接数的上限，若超过则需要删除部分连接
    int efConstruction = 80; // 搜索过程中候选节点集合的数量
    int m_L = max_L; // 节点的最高层数

    double grow = 0.2; // 节点的增长率


    std::set<std::pair<uint64_t, std::vector<float>>> deleted_nodes;// 已删除的键-向量对集合

    Node *entry; // 查找、插入的入口节点
    int getRandomLevel(); // 获取随机层数
};
