#pragma once
#include "HNSW.h"
#include <random>
#include <unordered_map>
#include <stack>
#include <queue>
#include <iostream>
#include <fstream>
#include "utils.h"


HNSWIndex::HNSWIndex(int M, int M_max, int efConstruction, Node* entry, int m_L) : M(M), M_max(M_max), efConstruction(efConstruction), entry(entry), m_L(m_L), gen(rd()) {
    deleted_nodes.clear();
}

HNSWIndex::HNSWIndex():gen(rd()) {
    entry = nullptr;
    deleted_nodes.clear();
}


HNSWIndex::~HNSWIndex() {
    //std::cout << "HNSWIndex: 开始销毁HNSW索引" << std::endl;

    if (!entry) {
        //std::cout << "HNSWIndex: 索引为空，无需清理" << std::endl;
        return;
    }
    
    Node *current = entry;
    // 在最底层DFS遍历所有节点，进行删除
    std::unordered_map<Node*, bool>visited;
    std::stack<Node*> stack;
    stack.push(entry);
    int deletedNodes = 0;
    
    //std::cout << "HNSWIndex: 开始遍历删除节点" << std::endl;
    while (!stack.empty()) {
        current = stack.top();
        stack.pop();
        if (visited[current]) continue;
        visited[current] = true;
        for (int i = 0; i < current->level; ++i) {
            for (Node *neighbor : current->neighbors[i]) {
                stack.push(neighbor);
            }
        }
        delete current;
        deletedNodes++;
    }
}



int HNSWIndex::getRandomLevel() {
    //std::cout << "HNSWIndex: 生成随机层级" << std::endl;
    std::geometric_distribution<> geometric(grow);

    // 生成层级并限制范围
    int level = std::min(1 + geometric(gen), m_L);
    //std::cout << "HNSWIndex: 生成的层级为 " << level << std::endl;
    return level;
}


// 接受一个嵌入向量和一个key，将其插入到HNSW图中
void HNSWIndex::insert(const std::vector<float>& embedding, uint64_t key) {
    if (!entry) {
        entry = new Node(getRandomLevel(), key, embedding);
        return;
    }

    int newLevel = getRandomLevel();
    Node *newNode = new Node(newLevel, key, embedding);


    // 从入口节点开始寻找
    Node *cur = entry;
    for (int i = entry->level - 1; i >= 0; i--) {
        // 自最顶层向下处理
        if (i >= newLevel) {
            // 在新节点的层数之上，贪心地更新cur，靠近新节点
            while (!cur->neighbors[i].empty()) {

                float best_sim = common_embd_similarity_cos(cur->embedding.data(), embedding.data(), cur->embedding.size());
                Node* best_neighbor = nullptr;
                for (auto it: cur->neighbors[i]) {
                    float sim = common_embd_similarity_cos(it->embedding.data(), embedding.data(), it->embedding.size());
                    if (sim > best_sim) { // 路径可以经过被删除的结点
                        best_sim = sim;
                        best_neighbor = it;
                    }
                }
                // 终止寻找
                if (!best_neighbor) break;
                cur = best_neighbor;// 更新cur，进入下一循环
            }
        }
        else {

            // 在新节点的层数之下，利用类似BFS的方法在每层中寻找与q最近邻的efConstruction个点，再从中选出M个最近邻进行连接
            std::priority_queue<std::pair<float, Node*>> queue;
            std::unordered_map<Node*, bool> visited;
            // 存放efConstruction个点的大顶堆
            std::priority_queue<std::pair<float, Node*>> pq;

            float sim = common_embd_similarity_cos(cur->embedding.data(), embedding.data(), cur->embedding.size());
            queue.push(std::make_pair(sim, cur));

            while (!queue.empty() && pq.size() < efConstruction) {
                Node *current = queue.top().second;
                queue.pop();
                if (visited[current]) continue;
                visited[current] = true;
                // 计算当前节点与新节点的相似度
                pq.push(std::make_pair(common_embd_similarity_cos(current->embedding.data(), embedding.data(), current->embedding.size()), current));
                // 将其邻居入队
                for (auto it: current->neighbors[i]) {
                    if (!visited[it]) { // 路径可以经过被删除的结点
                        queue.push(std::make_pair(common_embd_similarity_cos(it->embedding.data(), embedding.data(), it->embedding.size()), it));
                    }
                }
            }

            // 到此处，pq中存放了离待插入节点较近的efConstruciton个节点
            // 取出M个最相似的节点,与新节点建立连接
            int connectedNodes = 0;
            for (int j = 0; j < M && !pq.empty(); j++) {
                newNode->neighbors[i].push_back(pq.top().second);
                pq.pop();

                connectedNodes++;
            }

            // 连接邻居节点与新节点，并在必要时进行调整
            for (auto it: newNode->neighbors[i]) {
                it->neighbors[i].push_back(newNode);

                if (it->neighbors[i].size() > M_max) {
                    // 删除最远的一个连接
                    float worst_sim = 1;
                    Node *worst_neighbor = nullptr;
                    for (auto it2: it->neighbors[i]) {
                        float cursim = common_embd_similarity_cos(it2->embedding.data(), it->embedding.data(), it2->embedding.size());
                        if (cursim < worst_sim) {
                            worst_sim = cursim;
                            worst_neighbor = it2;
                        }
                    }
                    // 删除worst_neighbor的连接
                    auto remove = std::find(it->neighbors[i].begin(), it->neighbors[i].end(), worst_neighbor);
                    if (remove != it->neighbors[i].end()) {
                        it->neighbors[i].erase(remove);
                    }

                    auto remove_neighbor = std::find(worst_neighbor->neighbors[i].begin(), worst_neighbor->neighbors[i].end(), it);
                    if (remove_neighbor != worst_neighbor->neighbors[i].end()) {
                        worst_neighbor->neighbors[i].erase(remove_neighbor);
                    }
                }
            }
        }
    }

    if (newNode->level > entry->level)entry = newNode;
}

bool HNSWIndex::isInDeletedNodes(const std::vector<float>& query) {
    return deleted_nodes.contains(query);
}

void HNSWIndex::restoreDeletedNode(const std::vector<float>& query) {
    if (deleted_nodes.contains(query)) deleted_nodes.erase(query);
}



std::vector<uint64_t> HNSWIndex::search_knn_hnsw(const std::vector<float>& query, int k) {
    
    if (!entry) {
        return std::vector<uint64_t>();
    }

    // 从entry向下寻找，无法靠近query则进入下一层
    int curLevel = entry->level - 1;
    Node *cur = entry;
    
    while (curLevel >= 0 && !cur->neighbors[curLevel].empty()) {
        // 找到邻居中最相似的节点
        float best_sim = common_embd_similarity_cos(cur->embedding.data(), query.data(), cur->embedding.size());
        Node *best_neighbor = nullptr;
        for (auto it: cur->neighbors[curLevel]) {
            float sim = common_embd_similarity_cos(it->embedding.data(), query.data(), it->embedding.size());
            if (sim > best_sim) { // 路径可以经过被删除的结点
                best_sim = sim;
                best_neighbor = it;
            }
        }

        if (best_neighbor == nullptr) {
            curLevel--;
        } else {
            cur = best_neighbor;
        }
    }

    // 此时cur即为最接近query的节点
    // 在最底层进行knn搜索
    std::priority_queue<std::pair<float, Node*>> pq;// 大顶堆

    std::unordered_map<Node*, bool> visited;
    std::priority_queue<std::pair<float, Node*>> queue;
    queue.push(std::make_pair(common_embd_similarity_cos(cur->embedding.data(), query.data(), query.size()), cur));
    while (!queue.empty() && pq.size() < efConstruction) {
        Node *current = queue.top().second;
        queue.pop();
        if (visited[current]) continue;
        visited[current] = true;
        // 计算当前节点与query的相似度
        float sim = common_embd_similarity_cos(current->embedding.data(), query.data(), current->embedding.size());
        pq.push(std::make_pair(sim, current));
        // 将其邻居入队
        for (auto it: current->neighbors[0]) {
            if (!visited[it])queue.push(std::make_pair(common_embd_similarity_cos(it->embedding.data(), query.data(), query.size()),it));
        }
    }


    int visited_deleted_num = 0; // 访问的被删除结点的数量
    // 从优先级队列中取前k项不在deleted_node中的结点，作为最终输出
    std::vector<uint64_t> result;
    while (!pq.empty() && result.size() < k) {
        if ( !deleted_nodes.contains(pq.top().second->embedding) ) result.push_back(pq.top().second->key); // 若该结点未被删除，则加入结果
        else visited_deleted_num ++;
        pq.pop();
    }

    std::cout << "eliminate deleted nodes: " << visited_deleted_num << std::endl;

    return result;
}


void HNSWIndex::del(const std::vector<float>& vec) {
    deleted_nodes.insert(vec);
}

/**
 * @brief 将HNSW索引由内存存入指定目录
 * @param hnsw_data_root 存放HNSW持久化数据的根目录（最后需要带上‘/’）
*/
void HNSWIndex::saveToDisk(const std::string &hnsw_data_root) {

    // 维护被删的结点的key至embedding vector的映射
    std::unordered_map<uint32_t, std::vector<float>> deleted_nodes_store;

    // 待存入global_header的参数
    uint32_t M = this->M;
    uint32_t M_max = this->M_max;
    uint32_t efConstruction = this->efConstruction;
    uint32_t m_L = this->m_L;
    uint32_t max_level; // 全图最高层级
    uint32_t num_nodes;
    uint32_t dim = 768; // 嵌入向量的维度

    int nodeId = 0; // 保存结点的全局id
    // 检查结点数据文件夹
    std::string node_data_root = hnsw_data_root + "nodes";
    if ( utils::dirExists(node_data_root + "/") ) utils::rmdir(node_data_root.data());
    utils::mkdir((node_data_root + "/").data());


    if (entry) {
        max_level = entry->level;

        // 从最底层(Level 0)开始广度优先遍历，确保访问所有节点
        std::unordered_map<Node*, uint64_t> map;
        std::queue<Node*> q;
        q.push(entry);
        
        // 首先收集所有节点
        while (!q.empty()) {
            Node *current = q.front();
            q.pop();
            
            // 如果节点未被处理则分配ID
            if (map.find(current) == map.end()) {
                map[current] = nodeId;

                // 若该节点是被删的结点，则维护将其加入key至embedding vector的映射
                if (deleted_nodes.contains(current->embedding)) {
                    deleted_nodes_store[nodeId] = current->embedding;
                }

                // 更新nodeId
                nodeId++;

                // 处理该节点的所有层的邻居
                for (int l = 0; l < current->level; l++) {
                    for (auto neighbor : current->neighbors[l]) {
                        // 只将未处理的节点加入队列
                        if (map.find(neighbor) == map.end()) {
                            q.push(neighbor);
                        }
                    }
                }
            }
        }
        
        // 设置节点总数
        num_nodes = map.size();
        
        // HNSW中所有节点均已分配id，开始写入数据
        for (auto it: map) {
            // 该结点的指针和id
            uint64_t id = it.second;
            Node *cur = it.first;
            std::string node_dir = node_data_root + "/" + std::to_string(id);
            // 检查该id对应的目录
            if (utils::dirExists(node_dir + "/")) utils::rmdir(node_dir.data());
            utils::mkdir((node_dir + "/").data());

            // 写入参数文件header.bin
            std::string header_filename = node_dir + "/" + "header.bin";
            std::ofstream header_file(header_filename, std::ios::binary | std::ios::out | std::ios::trunc);
            if (!header_file.is_open()) {
                // 打开失败
                std::cerr << "无法打开文件进行写入: " << header_filename << std::endl;
                return;
            }
            header_file.write((const char *)&(cur->level), sizeof(uint32_t));
            header_file.write((const char *)&(cur->key), sizeof(uint64_t));
            //关闭文件
            header_file.close();


            // 检查邻接信息目录
            std::string neighbor_dir = node_dir + "/edges";
            if (utils::dirExists(neighbor_dir + "/")) utils::rmdir(neighbor_dir.data());
            utils::mkdir((neighbor_dir + "/").data());

            // 按层数依次写入文件
            for (int l = 0; l < cur->level; l++) {
                std::string neighbor_filename = neighbor_dir + "/" + std::to_string(l) + ".bin";
                std::ofstream neighbor_file(neighbor_filename, std::ios::binary | std::ios::out | std::ios::trunc);
                if (!neighbor_file.is_open()) {
                    // 打开失败
                    std::cerr << "无法打开文件进行写入: " << neighbor_filename << std::endl;
                    return;
                }

                uint32_t neighborNum = cur->neighbors[l].size();
                // 写入邻接信息
                neighbor_file.write((const char *)&neighborNum, sizeof(uint32_t));
                for (auto it: cur->neighbors[l]) {
                    // 将其id写入文件
                    uint32_t neighbor_id = map[it];
                    neighbor_file.write((const char *)&(neighbor_id), sizeof(uint32_t));
                }
                // 关闭文件
                neighbor_file.close();
            }
        }
    }
    else {
        // hnsw结构无结点
        max_level = 0;
        num_nodes = 0;
    }


    // 存放全局参数文件
    std::string global_header_filename = hnsw_data_root + "global_header.bin";
    // 二进制覆盖写模式打开global_header文件
    std::ofstream global_header_file(global_header_filename, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!global_header_file.is_open()) {
        // 打开失败
        std::cerr << "无法打开文件进行写入: " << global_header_filename << std::endl;
        return;
    }
    // 向global-header文件中写入HNSW参数
    global_header_file.write((const char *)(&M), sizeof(uint32_t));
    global_header_file.write((const char *)(&M_max), sizeof(uint32_t));
    global_header_file.write((const char *)(&efConstruction), sizeof(uint32_t));
    global_header_file.write((const char *)(&m_L), sizeof(uint32_t));
    global_header_file.write((const char *)(&max_level), sizeof(uint32_t));
    global_header_file.write((const char *)(&num_nodes), sizeof(uint32_t));
    global_header_file.write((const char *)(&dim), sizeof(uint32_t));

    // 关闭文件
    global_header_file.close();


    // 存放被删的结点们
    std::string deleted_nodes_filename = hnsw_data_root + "deleted_nodes.bin";
    std::ofstream deleted_nodes_file(deleted_nodes_filename, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!deleted_nodes_file.is_open()) {
        // 打开失败
        std::cerr << "无法打开文件进行写入: " << deleted_nodes_filename << std::endl;
        return;
    }
    // 写入被删除的结点的个数（uint32_t类型），再写入id，再写入embedding vector
    uint32_t deleted_nodes_num = deleted_nodes_store.size();
    deleted_nodes_file.write((const char *)(&deleted_nodes_num), sizeof(uint32_t));

    for (auto it: deleted_nodes_store) {
        // 写入id
        deleted_nodes_file.write((const char *)(&it.first), sizeof(uint32_t));
        // 写入向量
        deleted_nodes_file.write((const char *)(it.second.data()), dim * sizeof(float));
    }

}


