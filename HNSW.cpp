#pragma once
#include "HNSW.h"
#include <random>

HNSWIndex::HNSWIndex():gen(rd()) {

}

int HNSWIndex::getRandomLevel() {
    // 生成一个随机数，范围在0到m_L之间
    std::uniform_int_distribution<> dis(0, m_L - 1);
    return dis(gen);
}

void HNSWIndex::insert(const std::vector<float>& embedding, uint64_t key) {




}