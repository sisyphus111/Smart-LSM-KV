#pragma once
#include <random>
#include "skiplist.h"

double skiplist::my_rand(){
    /// 生成[0.0, 1.0)之间的随机数
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<double> dis(0.0, 1.0);
    return dis(gen);
}

int skiplist::randLevel(){
    int level = 0;
    while (my_rand() < p && level < MAX_LEVEL - 1) level++;
    return level;
}

void skiplist::insert(uint64_t key, const std::string &str){
    int level = randLevel();
    if (level > curMaxL) {
        for (int i = curMaxL; i <= level; ++i) {
            head->nxt[i] = tail;
        }
        curMaxL = level;
    }
    slnode *newNode = new slnode(key, str, NORMAL);
    std::vector<slnode *> update(MAX_LEVEL, nullptr);
    slnode *cur = head;
    for (int i = curMaxL; i >= 0; --i) {
        while (cur->nxt[i]->key < key) {
            cur = cur->nxt[i];
        }
        update[i] = cur;
        if(cur->nxt[i]->key == key) {
            //插入的key已存在，则更新值即可
            bytes -= cur->nxt[i]->val.size();
            bytes += str.size();

            cur->nxt[i]->val = str;
            delete newNode;
            return;
        }
    }
    for (int i = 0; i <= level; ++i) {
        newNode->nxt[i] = update[i]->nxt[i];
        update[i]->nxt[i] = newNode;
    }
    bytes += 12 + str.size();//key为64位，offset为32位，再加上value的大小
    return;
}

std::string skiplist::search(uint64_t key) {
    slnode *cur = head;
    for (int i = curMaxL; i >= 0; --i) {//从高到低遍历
        while (cur->nxt[i]->key < key) {
            cur = cur->nxt[i];
        }
        if(cur->nxt[i]->key == key) {
            return cur->nxt[i]->val;
        }
    }
    cur = cur->nxt[0];
    if (cur->key == key) {
        return cur->val;
    } else {
        return "";
    }
}

// bool skiplist::del(uint64_t key, uint32_t len) {
    
// }

void skiplist::scan(uint64_t key1, uint64_t key2, std::vector<std::pair<uint64_t, std::string>> &list) {
    //寻找key在key1到key2之间的所有元素
    slnode *cur = head;
    for (int i = curMaxL; i >= 0; --i) {
        while (cur->nxt[i]->key < key1) {
            cur = cur->nxt[i];
        }
    }
    cur = cur->nxt[0];
    while (cur->key <= key2) {
        if (cur->key >= key1) {
            list.push_back(std::make_pair(cur->key, cur->val));
        }
        cur = cur->nxt[0];
    }
}

slnode* skiplist::lowerBound(uint64_t key) {
    //寻找第一个大于key的元素？
    slnode *cur = head;
    for (int i = curMaxL; i >= 0; --i) {
        while (cur->nxt[i]->key < key) {
            cur = cur->nxt[i];
        }
    }
    return cur->nxt[0];
}

void skiplist::reset() {
    //重置跳表，删除所有元素
    slnode *cur = head->nxt[0];
    while (cur != tail) {
        slnode *next = cur->nxt[0];
        delete cur;
        cur = next;
    }
    for (int i = 0; i < MAX_LEVEL; ++i) {
        head->nxt[i] = tail;
    }
    bytes = 0x0;
    curMaxL = 1;
}

uint32_t skiplist::getBytes() {
    //返回跳表的字节数
    return bytes;
}