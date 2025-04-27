#pragma once

#define key_embedding_store "data/embedding.bin"
#define hnsw_path "data/hnsw_data_root/"
#define dim 768

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

    HNSWIndex* hnswIndex; // HNSW索引

    std::unordered_map<uint64_t, std::vector<float>> embeddings;// phase4，存放key-embedding对，支持磁盘读



public:
    KVStore(const std::string &dir);

    ~KVStore();

    void put(uint64_t key, const std::string &s) override;

    std::string get(uint64_t key) override;

    bool del(uint64_t key) override;

    void reset() override;
    void reset_key_embedding_store();

    void scan(uint64_t key1, uint64_t key2, std::list<std::pair<uint64_t, std::string>> &list) override;

    void compaction(int level = 0);// 默认合并第0层

    void delsstable(std::string filename);  // 从缓存中删除filename.sst， 并物理删除
    void addsstable(sstable ss, int level); // 将ss加入缓存

    std::string fetchString(std::string file, int startOffset, uint32_t len);

    // 持久化存储嵌入向量
    void save_embedding_to_disk(const std::string &filename = key_embedding_store);
    void load_embedding_from_disk(const std::string &data_root = key_embedding_store);

    // 持久化存储HNSW结构
    void save_hnsw_index_to_disk(const std::string &hnsw_data_root = hnsw_path);
    void load_hnsw_index_to_disk(const std::string &hnsw_data_root = hnsw_path);

    std::vector<std::pair<std::uint64_t, std::string>> search_knn(std::string query, int k);
    std::vector<std::pair<std::uint64_t, std::string>> search_knn_hnsw(std::string query, int k);
};
