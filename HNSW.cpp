#pragma once
#include "HNSW.h"
#include <random>
#include <unordered_map>
#include <stack>
#include <queue>
#include <iostream>

HNSWIndex::HNSWIndex():gen(rd()) {
    std::cout << "HNSWIndex: 创建新的HNSW索引" << std::endl;
    entry = nullptr;
}

HNSWIndex::~HNSWIndex() {
    std::cout << "HNSWIndex: 开始销毁HNSW索引" << std::endl;

    if (!entry) {
        std::cout << "HNSWIndex: 索引为空，无需清理" << std::endl;
        return;
    }
    
    Node *current = entry;
    // 在最底层DFS遍历所有节点，进行删除
    std::unordered_map<Node*, bool>visited;
    std::stack<Node*> stack;
    stack.push(entry);
    int deletedNodes = 0;
    
    std::cout << "HNSWIndex: 开始遍历删除节点" << std::endl;
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
    std::cout << "HNSWIndex: 共删除了 " << deletedNodes << " 个节点" << std::endl;
}

int HNSWIndex::getRandomLevel() {
    std::cout << "HNSWIndex: 生成随机层级" << std::endl;
    std::geometric_distribution<> geometric(grow);

    // 生成层级并限制范围
    int level = std::min(1 + geometric(gen), m_L);
    std::cout << "HNSWIndex: 生成的层级为 " << level << std::endl;
    return level;
}


// 接受一个嵌入向量和一个key，将其插入到HNSW图中
void HNSWIndex::insert(const std::vector<float>& embedding, uint64_t key) {
    std::cout << "HNSWIndex: 开始插入新节点，key=" << key << std::endl;
    int newLevel = getRandomLevel();

    std::cout << "HNSWIndex: 创建新节点，层级=" << newLevel << std::endl;
    Node *newNode = new Node(newLevel, key, embedding);
    if (!entry) {
        std::cout << "HNSWIndex: 首个节点插入，设为入口点" << std::endl;
        entry = newNode;
        return;
    }

    // 从入口节点开始寻找
    Node *cur = entry;
    std::cout << "HNSWIndex: 从第 " << std::max(entry->level, newLevel) - 1 << " 层开始搜索最近节点" << std::endl;
    for (int i = std::max(entry->level, newLevel) - 1; i >= 0; i--) {
        // 自最顶层向下处理
        if (i > newLevel - 1) {
            std::cout << "HNSWIndex: 第 " << i << " 层 (高于新节点层级)，贪心搜索" << std::endl;
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
                std::cout << "HNSWIndex: 移动到更近的节点，相似度=" << best_sim << std::endl;
                cur = best_neighbor;// 更新cur，进入下一循环
            }
        }
        else {
            std::cout << "HNSWIndex: 第 " << i << " 层 (新节点层级范围内)，BFS搜索近邻" << std::endl;
            // 在新节点的层数之下，利用类似BFS的方法在每层中寻找与q最近邻的efConstruction个点，再从中选出M个最近邻进行连接
            std::queue<Node*> queue;
            std::unordered_map<Node*, bool> visited;

            // 存放efConstruction个点的大顶堆
            std::priority_queue<std::pair<float, Node*>> pq;


            queue.push(cur);
            while (!queue.empty() && pq.size() < efConstruction) {
                Node *current = queue.front();
                queue.pop();
                if (visited[current]) continue;
                visited[current] = true;
                // 计算当前节点与新节点的相似度
                float sim = common_embd_similarity_cos(current->embedding.data(), embedding.data(), current->embedding.size());
                pq.push(std::make_pair(sim, current));
                // 将其邻居入队
                for (auto it: current->neighbors[i]) {
                    if (!visited[it])queue.push(it);
                }
            }

            std::cout << "HNSWIndex: 找到 " << pq.size() << " 个候选近邻节点" << std::endl;
            // 到此处，pq中存放了离待插入节点较近的efConstruciton个节点
            // 取出M个最相似的节点,与新节点建立连接
            int connectedNodes = 0;
            for (int j = 0; j < M && !pq.empty(); j++) {
                newNode->neighbors[i].push_back(pq.top().second);
                pq.pop();

                connectedNodes++;
            }
            std::cout << "HNSWIndex: 新节点在第 " << i << " 层连接了 " << connectedNodes << " 个邻居" << std::endl;
            
            // 连接邻居节点与新节点，并在必要时进行调整
            int pruned = 0;
            for (auto it: newNode->neighbors[i]) {
                it->neighbors[i].push_back(newNode);

                if (it->neighbors[i].size() > M_max) {
                    std::cout << "HNSWIndex: 节点邻居数量超过上限，需要修剪" << std::endl;
                    // 删除最远的一个连接
                    float worst_sim = 1;
                    Node *worst_neighbor = nullptr;
                    for (auto it2: it->neighbors[i]) {
                        float sim = common_embd_similarity_cos(it2->embedding.data(), it->embedding.data(), it2->embedding.size());
                        if (sim < worst_sim) {
                            worst_sim = sim;
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
                std::cout << "HNSWIndex: 共修剪了 " << pruned << " 个连接" << std::endl;
            }
        }
    }

    // 若新节点的层数大于当前层数，则更新当前层数
    if (newLevel > entry->level) {
        std::cout << "HNSWIndex: 新节点层级(" << newLevel << ")大于入口节点层级(" << entry->level << ")，更新入口点" << std::endl;
        entry = newNode;
    }
    
    std::cout << "HNSWIndex: 节点插入完成，key=" << key << std::endl;
}

std::vector<uint64_t> HNSWIndex::search_knn_hnsw(const std::vector<float>& query, int k) {
    std::cout << "HNSWIndex: 开始KNN搜索，k=" << k << std::endl;
    
    if (!entry) {
        std::cout << "HNSWIndex: 索引为空，返回空结果" << std::endl;
        return std::vector<uint64_t>();
    }

    // 从entry向下寻找，无法靠近query则进入下一层
    int curLevel = entry->level - 1;
    Node *cur = entry;
    std::cout << "HNSWIndex: 从第 " << curLevel << " 层开始搜索" << std::endl;
    
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
            std::cout << "HNSWIndex: 第 " << curLevel << " 层没有更近的节点，进入下一层" << std::endl;
            curLevel--;
        } else {
            std::cout << "HNSWIndex: 移动到更相似的节点，相似度=" << best_sim << std::endl;
            cur = best_neighbor;
        }
    }

    std::cout << "HNSWIndex: 在底层开始KNN搜索" << std::endl;
    // 此时cur即为最接近query的节点

    // 在最底层进行knn搜索
    std::priority_queue<std::pair<float, Node*>> pq;// 大顶堆

    std::unordered_map<Node*, bool> visited;
    std::queue<Node*> queue;
    queue.push(cur);
    while (!queue.empty() && pq.size() < efConstruction) {
        Node *current = queue.front();
        queue.pop();
        if (visited[current]) continue;
        visited[current] = true;
        // 计算当前节点与query的相似度
        float sim = common_embd_similarity_cos(current->embedding.data(), query.data(), current->embedding.size());
        pq.push(std::make_pair(sim, current));
        // 将其邻居入队
        for (auto it: current->neighbors[0]) {
            if (!visited[it])queue.push(it);
        }
    }
    
    std::cout << "HNSWIndex: 共找到 " << pq.size() << " 个候选近邻节点" << std::endl;

    // 从优先级队列中取前k项，作为最终输出
    std::vector<uint64_t> result;
    while (!pq.empty() && result.size() < k) {
        result.push_back(pq.top().second->key);
        pq.pop();
    }

    std::cout << "HNSWIndex: 返回 " << result.size() << " 个最近邻结果" << std::endl;
    return result;
}
