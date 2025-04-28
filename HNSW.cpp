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
    entry = nullptr;
    deleted_nodes.clear();
}

HNSWIndex::HNSWIndex():gen(rd()) {
    //std::cout << "HNSWIndex: 创建新的HNSW索引" << std::endl;
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
    //std::cout << "HNSWIndex: 共删除了 " << deletedNodes << " 个节点" << std::endl;
}

void HNSWIndex::simulated_annealing_select(
    std::vector<std::pair<float, Node*>>& scored_candidates,
    Node* center,
    int current_level,
    float temperature
) {
    // 按相似度降序排序
    std::sort(scored_candidates.begin(), scored_candidates.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });

    // 3. 模拟退火选择
    float avg_sim = std::accumulate(
        scored_candidates.begin(), scored_candidates.end(), 0.0f,
        [](float sum, const auto& p) { return sum + p.first; }
    ) / scored_candidates.size();

    std::uniform_real_distribution<float> dist(0, 1);
    for (const auto& [sim, node] : scored_candidates) {
        // 计算能量差（这里用相似度差值）
        float delta = sim - avg_sim;

        // 退火概率公式：p = exp(ΔE / T)
        float accept_prob = std::exp(delta / temperature);

        // 接受条件：1) 概率达标 或 2) 强制保留Top 2节点
        if (accept_prob > dist(gen) || center->neighbors[current_level].size() < 2) {
            center->neighbors[current_level].push_back(node);
            if (center->neighbors[current_level].size() >= M) break;
        }
    }

    return;
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
        //std::cout << "HNSWIndex: 首个节点插入，设为入口点" << std::endl;
        entry = new Node(getRandomLevel(), key, embedding);
        return;
    }


    //std::cout << "HNSWIndex: 开始插入新节点，key=" << key << std::endl;
    int newLevel = getRandomLevel();

    //std::cout << "HNSWIndex: 创建新节点，层级=" << newLevel << std::endl;
    Node *newNode = new Node(newLevel, key, embedding);


    // 从入口节点开始寻找
    Node *cur = entry;
    //std::cout << "HNSWIndex: 从第 " << std::max(entry->level, newLevel) - 1 << " 层开始搜索最近节点" << std::endl;
    for (int i = entry->level - 1; i >= 0; i--) {
        // 自最顶层向下处理
        if (i >= newLevel) {
            //std::cout << "HNSWIndex: 第 " << i << " 层 (高于新节点层级)，贪心搜索" << std::endl;
            // 在新节点的层数之上，贪心地更新cur，靠近新节点
            while (!cur->neighbors[i].empty()) {

                float best_sim = common_embd_similarity_cos(cur->embedding.data(), embedding.data(), cur->embedding.size());
                Node* best_neighbor = nullptr;
                for (auto it: cur->neighbors[i]) {
                    float sim = common_embd_similarity_cos(it->embedding.data(), embedding.data(), it->embedding.size());
                    if (sim > best_sim) {
                        best_sim = sim;
                        best_neighbor = it;
                    }
                }
                // 终止寻找
                if (!best_neighbor) break;
                //std::cout << "HNSWIndex: 移动到更近的节点，相似度=" << best_sim << std::endl;
                cur = best_neighbor;// 更新cur，进入下一循环
            }
        }
        else {
            //std::cout << "HNSWIndex: 第 " << i << " 层 (新节点层级范围内)，BFS搜索近邻" << std::endl;
            // 在新节点的层数之下，利用类似BFS的方法在每层中寻找与q最近邻的efConstruction个点，再从中选出M个最近邻进行连接
            std::priority_queue<std::pair<float, Node*>> queue;
            std::unordered_map<Node*, bool> visited;
            // 存放efConstruction个点的大顶堆
            std::vector<std::pair<float, Node*>> pq;


            queue.push(std::make_pair(common_embd_similarity_cos(embedding.data(), cur->embedding.data(), embedding.size()), cur));

            while (!queue.empty() && pq.size() < efConstruction) {
                Node *current = queue.top().second;
                queue.pop();
                if (visited[current]) continue;
                visited[current] = true;
                // 计算当前节点与新节点的相似度
                pq.push_back(std::make_pair(common_embd_similarity_cos(current->embedding.data(), embedding.data(), current->embedding.size()), current));
                // 将其邻居入队
                for (auto it: current->neighbors[i]) {
                    if (!visited[it]) {
                        queue.push(std::make_pair(common_embd_similarity_cos(embedding.data(), it->embedding.data(), embedding.size()), it));
                    }
                }
            }

            //std::cout << "HNSWIndex: 找到 " << pq.size() << " 个候选近邻节点" << std::endl;
            // 到此处，pq中存放了离待插入节点较近的efConstruciton个节点
            // 取出M个最相似的节点,与新节点建立连接
            simulated_annealing_select(pq, newNode, i, 1.0f);

            //std::cout << "HNSWIndex: 新节点在第 " << i << " 层连接了 " << connectedNodes << " 个邻居" << std::endl;
            
            // 连接邻居节点与新节点，并在必要时进行调整
            int pruned = 0;
            for (auto it: newNode->neighbors[i]) {
                it->neighbors[i].push_back(newNode);

                if (it->neighbors[i].size() > M_max) {
                    //std::cout << "HNSWIndex: 节点邻居数量超过上限，需要修剪" << std::endl;
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
                    pruned++;
                }
            }
            if(pruned > 0) {
                //std::cout << "HNSWIndex: 共修剪了 " << pruned << " 个连接" << std::endl;
            }
        }
    }


    // 若插入的结点比入口结点高，则将entry指向新节点
    if (newLevel > entry->level) {
        //std::cout << "HNSWIndex: 新节点高于入口节点，更新入口节点" << std::endl;
        entry = newNode;
    }
    //std::cout << "HNSWIndex: 节点插入完成，key=" << key << std::endl;
}

std::vector<uint64_t> HNSWIndex::search_knn_hnsw(const std::vector<float>& query, int k) {
    //std::cout << "HNSWIndex: 开始KNN搜索，k=" << k << std::endl;
    
    if (!entry) {
        //std::cout << "HNSWIndex: 索引为空，返回空结果" << std::endl;
        return std::vector<uint64_t>();
    }

    // 从entry向下寻找，无法靠近query则进入下一层
    int curLevel = entry->level - 1;
    Node *cur = entry;
    //std::cout << "HNSWIndex: 从第 " << curLevel << " 层开始搜索" << std::endl;
    
    while (curLevel >= 0 && !cur->neighbors[curLevel].empty()) {
        // 找到邻居中最相似的节点
        float best_sim = common_embd_similarity_cos(cur->embedding.data(), query.data(), cur->embedding.size());
        Node *best_neighbor = nullptr;
        for (auto it: cur->neighbors[curLevel]) {
            float sim = common_embd_similarity_cos(it->embedding.data(), query.data(), it->embedding.size());
            if (sim > best_sim) {
                best_sim = sim;
                best_neighbor = it;
            }
        }

        if (best_neighbor == nullptr) {
            //std::cout << "HNSWIndex: 第 " << curLevel << " 层没有更近的节点，进入下一层" << std::endl;
            curLevel--;
        } else {
            //std::cout << "HNSWIndex: 移动到更相似的节点，相似度=" << best_sim << std::endl;
            cur = best_neighbor;
        }
    }

    //std::cout << "HNSWIndex: 在底层开始KNN搜索" << std::endl;
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
    
    //std::cout << "HNSWIndex: 共找到 " << pq.size() << " 个候选近邻节点" << std::endl;

    // 从优先级队列中取前k项不在deleted_node中的结点，作为最终输出
    std::vector<uint64_t> result;
    while (!pq.empty() && result.size() < k) {
        if ( !deleted_nodes.contains(pq.top().second->embedding) )result.push_back(pq.top().second->key); // 若该结点未被删除，则加入结果
        pq.pop();
    }

    //std::cout << "HNSWIndex: 返回 " << result.size() << " 个最近邻结果" << std::endl;
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

        // 从entry结点开始遍历，写入各个结点的数据，并维护结点至id的映射
        std::unordered_map<Node*, uint64_t> map;

        // 从入口结点开始，在每层进行DFS遍历，建立所有结点与id的映射
        for (int i = 0; i < entry->level; i++) {

            std::queue<Node*> q;
            q.push(entry);

            while (!q.empty()) {
                Node *current = q.front();
                q.pop();
                if (map.find(current) == map.end() && !deleted_nodes.contains(current->embedding)) {// 还未访问到，且未被删除
                    // 为结点分配id
                    map[current] = nodeId++;
                }
                // 将该层的未被访问邻居入队
                for (auto it: current->neighbors[i]) {
                    if (map.find(it) == map.end()) {
                        q.push(it);
                    }
                }
            }
        }
        // 此时nodeId即为结点的总数
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
                // 减去被删除的邻居
                for (auto it: cur->neighbors[l]) {
                    if (deleted_nodes.contains(it->embedding)) {
                        neighborNum--;
                    }
                }


                // 写入邻接信息
                neighbor_file.write((const char *)&neighborNum, sizeof(uint32_t));
                for (auto it: cur->neighbors[l]) {
                    if (deleted_nodes.contains(it->embedding)) continue;

                    // 未被删除，则将其id写入文件
                    uint64_t neighbor_id = map[it];
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

    // 最后存放全局参数文件
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
}


