#pragma once

#include "kvstore_api.h"
#include "skiplist.h"
#include "sstable.h"
#include "sstablehead.h"
#include "embedding.h"
#include "HNSW.h"

#include <map>
#include <set>
#include <unordered_map>

class KVStore : public KVStoreAPI {
    // You can add your implementation here
    
private:
    skiplist *s = new skiplist(0.5); // memtable
    // std::vector<sstablehead> sstableIndex;  // sstable的表头缓存

    std::vector<sstablehead> sstableIndex[15]; // the sshead for each level

    int totalLevel = -1; // 层数

    std::set<uint64_t> cacheKey; // 缓存所有键
    std::unordered_map<uint64_t, std::vector<float>> cacheEmbedding; // 缓存所有嵌入向量

    HNSWIndex* hnswIndex; // HNSW索引，存储所有键值对
    std::set<uint64_t> cacheKey_HNSW;// key的缓冲区，在search_knn_show时批量计算嵌入向量


public:
    KVStore(const std::string &dir);

    ~KVStore();

    void put(uint64_t key, const std::string &s) override;

    std::string get(uint64_t key) override;

    bool del(uint64_t key) override;

    void reset() override;

    void scan(uint64_t key1, uint64_t key2, std::list<std::pair<uint64_t, std::string>> &list) override;

    void compaction(int level = 0);// 默认合并第0层

    void delsstable(std::string filename);  // 从缓存中删除filename.sst， 并物理删除
    void addsstable(sstable ss, int level); // 将ss加入缓存

    std::string fetchString(std::string file, int startOffset, uint32_t len);

    std::vector<std::pair<std::uint64_t, std::string>> search_knn(std::string query, int k);
    std::vector<std::pair<std::uint64_t, std::string>> search_knn_hnsw(std::string query, int k);

    // 接受向量的search_knn和search_knn_hnsw函数，phase3测试用
    std::vector<std::pair<std::uint64_t, std::string>> search_knn_vector(const std::vector<float> &query, int k);
    std::vector<std::pair<std::uint64_t, std::string>> search_knn_hnsw_vector(const std::vector<float> &query, int k);
};
