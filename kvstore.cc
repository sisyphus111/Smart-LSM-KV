#include "kvstore.h"

#include "skiplist.h"
#include "sstable.h"
#include "utils.h"

#include <algorithm>
#include <chrono>  // 添加chrono库用于精确计时
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <queue>
#include <string>
#include <utility>

static const std::string DEL = "~DELETED~";
const uint32_t MAXSIZE       = 2 * 1024 * 1024;

struct poi {
    int sstableId; // vector中第几个sstable
    int pos;       // 该sstable的第几个key-offset
    uint64_t time;
    Index index;
};

struct cmpPoi {
    bool operator()(const poi &a, const poi &b) {
        if (a.index.key == b.index.key)
            return a.time < b.time;
        return a.index.key > b.index.key;
    }
};

KVStore::KVStore(const std::string &dir) :
    KVStoreAPI(dir) // read from sstables
{
    util.init();
    hnswIndex = new HNSWIndex();
    for (totalLevel = 0;; ++totalLevel) {
        std::string path = dir + "/level-" + std::to_string(totalLevel) + "/";
        std::vector<std::string> files;
        if (!utils::dirExists(path)) {
            totalLevel--;
            break; // stop read
        }
        int nums = utils::scanDir(path, files);
        sstablehead cur;
        for (int i = 0; i < nums; ++i) {       // 读每一个文件头
            std::string url = path + files[i]; // url, 每一个文件名
            cur.loadFileHead(url.data());
            sstableIndex[totalLevel].push_back(cur);
            TIME = std::max(TIME, cur.getTime()); // 更新时间戳
        }
    }

    // 启动时加载HNSW
    // load_embedding_from_disk();
    load_hnsw_index_from_disk();

}

KVStore::~KVStore()
{

    // 退出时保存嵌入向量和HNSW索引
    save_embedding_to_disk();
    save_hnsw_index_to_disk();


    sstable ss(s);
    if (!ss.getCnt())
        return; // empty sstable
    std::string path = std::string("./data/level-0/");
    if (!utils::dirExists(path)) {
        utils::_mkdir(path.data());
        totalLevel = 0;
    }
    ss.putFile(ss.getFilename().data());
    compaction(); // 从0层开始尝试合并
}

/**
 * Insert/Update the key-value pair.
 * No return values for simplicity.
 */
void KVStore::put(uint64_t key, const std::string &val) {
    // put操作的不同情况：
    if (val == DEL) {
        // 当前为删除操作
        embeddings[key] = std::vector<float>(vec_dim, std::numeric_limits<float>::max());
        // hnswIndex需分类讨论：之前已添加键值对，则对相应的键-嵌入向量进行删除；若原来没有该键，则不用操作
        if (get(key) != "") hnswIndex->del(key, getEmbd(get(key)));
    }
    else {
        // 当前为“添加”或“更新”操作，向量只需更新即可
        embeddings[key] = getEmbd(val);
        // hnswIndex需分类讨论：更新值/更新后恢复/如加/添加/删除后恢复
        // 更新值：原键在LSM-Tree中存在，key-get(key)和key-val都不存在于deleted nodes中，且get(key)不等于val
        // 更新后恢复：原键在LSM-Tree中存在，key-get(key)必不存在于deleted nodes，key-val存在于deleted nodes中
        // 如加：原键在LSM-Tree中存在，且key-val在deleted nodes中不存在，且get(key)等于val
        // 添加：原键在LSM-Tree中不存在，且key-val在deleted nodes中不存在
        // 删除后恢复：原键在LSM-Tree中不存在，但key-val在deleted nodes中存在
        if (get(key) != "") {
            // 更新值/更新后恢复/如加
            if (get(key) != val && !hnswIndex->isInDeletedNodes(key, getEmbd(val))) {
                // 更新值
                hnswIndex->del(key, getEmbd(get(key))); // 删除原值
                hnswIndex->insert(getEmbd(val), key); // 插入新值
            }
            else if (hnswIndex->isInDeletedNodes(key, getEmbd(val))) {
                // 更新后恢复
                hnswIndex->restoreDeletedNode(key, getEmbd(val)); // 恢复更新前的值
                hnswIndex->del(key, getEmbd(get(key))); // 删除更新后的值
            }
            else if (get(key) == val && !hnswIndex->isInDeletedNodes(key, getEmbd(val))) {
                // 如加
            }
        }
        else {
            // 添加/删除后恢复
            if (get(key) == "" && !hnswIndex->isInDeletedNodes(key, getEmbd(val))) {
                // 添加
                hnswIndex->insert(getEmbd(val), key);
            }
            else if (get(key) == "" && hnswIndex->isInDeletedNodes(key, getEmbd(val))) {
                // 删除后恢复
                hnswIndex->restoreDeletedNode(key, getEmbd(val));
            }
        }
    }

    uint32_t nxtsize = s->getBytes();
    std::string res  = s->search(key);
    if (!res.length()) { // new add
        nxtsize += 12 + val.length();
    } else
        nxtsize = nxtsize - res.length() + val.length(); // change string
    if (nxtsize + 10240 + 32 <= MAXSIZE)
        s->insert(key, val); // 小于等于（不超过） 2MB
    else {
        // 持久化跳表时，把嵌入向量持久化
        save_embedding_to_disk();

        sstable ss(s);
        s->reset();
        std::string url  = ss.getFilename();
        std::string path = "./data/level-0";
        if (!utils::dirExists(path)) {
            utils::mkdir(path.data());
            totalLevel = 0;
        }
        addsstable(ss, 0);      // 加入缓存
        ss.putFile(url.data()); // 加入磁盘
        compaction();
        s->insert(key, val);
    }

}

/**
 * Returns the (string) value of the given key.
 * An empty string indicates not found.
 */
std::string KVStore::get(uint64_t key) //
{
    uint64_t time = 0;
    int goalOffset;
    uint32_t goalLen;
    std::string goalUrl;
    std::string res = s->search(key);
    if (res.length()) { // 在memtable中找到, 或者是deleted，说明最近被删除过，
                        // 不用查sstable
        if (res == DEL)
            return "";
        return res;
    }
    for (int level = 0; level <= totalLevel; ++level) {
        for (sstablehead it : sstableIndex[level]) {
            if (key < it.getMinV() || key > it.getMaxV())
                continue;
            uint32_t len;
            int offset = it.searchOffset(key, len);
            if (offset == -1) {
                if (!level)
                    continue;
                else
                    break;
            }
            // sstable ss;
            // ss.loadFile(it.getFilename().data());
            if (it.getTime() > time) { // find the latest head
                time       = it.getTime();
                goalUrl    = it.getFilename();
                goalOffset = offset + 32 + 10240 + 12 * it.getCnt();
                goalLen    = len;
            }
        }
        if (time)
            break; // only a test for found
    }
    if (!goalUrl.length())
        return ""; // not found a sstable
    res = fetchString(goalUrl, goalOffset, goalLen);
    if (res == DEL)
        return "";
    return res;
}

/**
 * Delete the given key-value pair if it exists.
 * Returns false iff the key is not found.
 */
bool KVStore::del(uint64_t key) {
    std::string res = get(key);
    if (!res.length())
        return false; // not exist

    // 删除涉及的具体操作在put函数中进行
    put(key, DEL);    // put a del marker


    return true;
}

/**
 * This resets the kvstore. All key-value pairs should be removed,
 * including memtable and all sstables files.
 */
void KVStore::reset() {
    // 清空嵌入向量的持久化存储和内存存储
    reset_key_embedding_store();
    embeddings.clear();
    // 清空HNSW索引的持久化存储和内存存储
    if (utils::dirExists(hnsw_dir)) utils::rmdir(hnsw_dir_name);
    if (hnswIndex)delete hnswIndex;
    hnswIndex = new HNSWIndex();

    s->reset(); // 先清空memtable
    std::vector<std::string> files;
    for (int level = 0; level <= totalLevel; ++level) { // 依层清空每一层的sstables
        std::string path = std::string("./data/level-") + std::to_string(level);
        int size         = utils::scanDir(path, files);
        for (int i = 0; i < size; ++i) {
            std::string file = path + "/" + files[i];
            utils::rmfile(file.data());
        }
        utils::rmdir(path.data());
        sstableIndex[level].clear();
    }
    totalLevel = -1;



}

/**
 * @brief 重置disk中的键-嵌入向量存储，将其覆盖为：仅开头8Byte为768（维数）
 */
void KVStore::reset_key_embedding_store() {
    // 打开文件，以二进制写入模式，并且需要覆盖原内容
    std::ofstream ofs(key_embedding_store, std::ios::binary | std::ios::trunc);

    // 检查文件是否成功打开
    if (!ofs.is_open()) {
        std::cerr << "无法打开文件: " << key_embedding_store << std::endl;
        return;
    }

    // 写入表示嵌入向量维度的值(768)到文件开头8字节处
    uint64_t dimension = 768;
    ofs.write(reinterpret_cast<char*>(&dimension), sizeof(uint64_t));

    // 关闭文件
    ofs.close();
}

/**
 * Return a list including all the key-value pair between key1 and key2.
 * keys in the list should be in an ascending order.
 * An empty string indicates not found.
 */

struct myPair {
    uint64_t key, time;
    int id, index;
    std::string filename;

    myPair(uint64_t key, uint64_t time, int index, int id,
           std::string file) { // construct function
        this->time     = time;
        this->key      = key;
        this->id       = id;
        this->index    = index;
        this->filename = file;
    }
};

struct cmp {
    bool operator()(myPair &a, myPair &b) {
        if (a.key == b.key)
            return a.time < b.time;
        return a.key > b.key;
    }
};


void KVStore::scan(uint64_t key1, uint64_t key2, std::list<std::pair<uint64_t, std::string>> &list) {
    std::vector<std::pair<uint64_t, std::string>> mem;
    // std::set<myPair> heap; // 维护一个指针最小堆
    std::priority_queue<myPair, std::vector<myPair>, cmp> heap;
    // std::vector<sstable> ssts;
    std::vector<sstablehead> sshs;
    s->scan(key1, key2, mem);   // add in mem
    std::vector<int> head, end; // [head, end)
    int cnt = 0;
    if (mem.size())
        heap.push(myPair(mem[0].first, INF, 0, -1, "qwq"));
    for (int level = 0; level <= totalLevel; ++level) {
        for (sstablehead it : sstableIndex[level]) {
            if (key1 > it.getMaxV() || key2 < it.getMinV())
                continue; // 无交集
            int hIndex = it.lowerBound(key1);
            int tIndex = it.lowerBound(key2);
            if (hIndex < it.getCnt()) { // 此sstable可用
                // sstable ss; // 读sstable
                std::string url = it.getFilename();
                // ss.loadFile(url.data());

                heap.push(myPair(it.getKey(hIndex), it.getTime(), hIndex, cnt++, url));
                head.push_back(hIndex);
                if (it.search(key2) == tIndex)
                    tIndex++; // tIndex为第一个不可的
                end.push_back(tIndex);
                // ssts.push_back(ss); // 加入ss
                sshs.push_back(it);
            }
        }
    }
    uint64_t lastKey = INF; // only choose the latest key
    while (!heap.empty()) { // 维护堆
        myPair cur = heap.top();
        heap.pop();
        if (cur.id >= 0) { // from sst
            if (cur.key != lastKey) {
                lastKey         = cur.key;
                uint32_t start  = sshs[cur.id].getOffset(cur.index - 1);
                uint32_t len    = sshs[cur.id].getOffset(cur.index) - start;
                uint32_t scnt   = sshs[cur.id].getCnt();
                std::string res = fetchString(cur.filename, 10240 + 32 + scnt * 12 + start, len);
                if (res.length() && res != DEL)
                    list.emplace_back(cur.key, res);
            }
            if (cur.index + 1 < end[cur.id]) { // add next one to heap
                heap.push(myPair(sshs[cur.id].getKey(cur.index + 1), cur.time, cur.index + 1, cur.id, cur.filename));
            }
        } else { // from mem
            if (cur.key != lastKey) {
                lastKey         = cur.key;
                std::string res = mem[cur.index].second;
                if (res.length() && res != DEL)
                    list.emplace_back(cur.key, mem[cur.index].second);
            }
            if (cur.index < mem.size() - 1) {
                heap.push(myPair(mem[cur.index + 1].first, cur.time, cur.index + 1, -1, cur.filename));
            }
        }
    }
}

void KVStore::compaction(int level) {
    // 从第0层合并时，若文件个数未超阈值，则不需要合并
    if (level == 0 && sstableIndex[0].size() <= 2) return;

    // 若下一层目录不存在则创建
    std::string targetLevelPath = "./data/level-" + std::to_string(level + 1);
    if (!utils::dirExists(targetLevelPath)) {
        utils::mkdir(targetLevelPath.c_str());
        if (totalLevel < level + 1) totalLevel = level + 1;
    }

    // 从当前层选择SSTable进行合并
    std::vector<sstablehead> selectedTables;
    uint64_t minKey = INF;
    uint64_t maxKey = 0;

    if (level == 0) {
        // 第0层，全部合并
        for (int i = 0; i < sstableIndex[0].size(); i++) {
            selectedTables.push_back(sstableIndex[0][i]);
            minKey = std::min(minKey, sstableIndex[0][i].getMinV());
            maxKey = std::max(maxKey, sstableIndex[0][i].getMaxV());
        }
    } else {
        // 其他层，取4个进行合并
        int filesToMerge = std::min(4, (int)sstableIndex[level].size());
        for (int i = 0; i < filesToMerge; i++) {
            selectedTables.push_back(sstableIndex[level][i]);
            minKey = std::min(minKey, sstableIndex[level][i].getMinV());
            maxKey = std::max(maxKey, sstableIndex[level][i].getMaxV());
        }
    }

    // 错误：没有选中任何SSTable
    if (selectedTables.empty()) return;

    // 寻找下一层有重叠的SSTable，进行合并
    if (level + 1 <= totalLevel) {
        for (auto &sshead : sstableIndex[level + 1]) {
            // Check for key range overlap
            if (!(maxKey < sshead.getMinV() || minKey > sshead.getMaxV())) {
                selectedTables.push_back(sshead);
            }
        }
    }

    // 准备多路合并数据结构
    std::priority_queue<poi, std::vector<poi>, cmpPoi> pq;
    std::vector<sstable> tables(selectedTables.size());

    // 将待合并SSTable全部加载到内存
    for (size_t i = 0; i < selectedTables.size(); i++) {
        tables[i].loadFile(selectedTables[i].getFilename().c_str());

        // 初始化优先级队列，加入每个SSTable的第一个条目
        if (tables[i].getCnt() > 0) {
            poi entry;
            entry.sstableId = i;
            entry.pos = 0;
            entry.time = tables[i].getTime();
            entry.index = tables[i].getIndexById(0);
            pq.push(entry);
        }
    }

    // 创建用于存储合并结果的SSTable
    sstable newTable;
    newTable.reset();
    newTable.setTime(++TIME);
    std::string outPath = targetLevelPath + "/" + std::to_string(TIME) + ".sst";
    newTable.setFilename(outPath);


    uint64_t lastKey = INF;

    // 多路合并主循环
    while (!pq.empty()) {
        poi current = pq.top();
        pq.pop();

        uint64_t key = current.index.key;
        int tableId = current.sstableId;
        int pos = current.pos;

        //当前处理条目的值
        std::string value = tables[tableId].getData(pos);


        if (key == lastKey) {
            // 若有重复则跳过
        } else {
            // 下一层是不是最底层——删除标记是否可以去掉
            bool isDeepestLevel = (level + 1 == totalLevel);


            if (value != DEL || !isDeepestLevel) {
                // 检查.sst文件是否即将超过2MB
                if (newTable.getBytes() + 12 + value.size() > MAXSIZE) {
                    newTable.putFile(newTable.getFilename().c_str());
                    addsstable(newTable, level + 1);

                    // 重置
                    newTable.reset();
                    newTable.setTime(++TIME);
                    outPath = targetLevelPath + "/" + std::to_string(TIME) + ".sst";
                    newTable.setFilename(outPath);
                }

                newTable.insert(key, value);
            }
            lastKey = key;
        }

        // 加入下一个元素
        if (pos + 1 < tables[tableId].getCnt()) {
            poi next;
            next.sstableId = tableId;
            next.pos = pos + 1;
            next.time = current.time;
            next.index = tables[tableId].getIndexById(pos + 1);
            pq.push(next);
        }
    }

    // 处理最后一个
    if (newTable.getCnt() > 0) {
        newTable.putFile(newTable.getFilename().c_str());
        addsstable(newTable, level + 1);
    }

    // 删除所有归并文件
    for (size_t i = 0; i < selectedTables.size(); i++) {
        delsstable(selectedTables[i].getFilename());
    }

    // 检查下一层是否需要归并
    int nextLevelThreshold = 1 << (level + 2);
    if (sstableIndex[level + 1].size() > nextLevelThreshold) {
        compaction(level + 1);
    }
}



void KVStore::delsstable(std::string filename) {
    for (int level = 0; level <= totalLevel; ++level) {
        int size = sstableIndex[level].size(), flag = 0;
        for (int i = 0; i < size; ++i) {
            if (sstableIndex[level][i].getFilename() == filename) {
                sstableIndex[level].erase(sstableIndex[level].begin() + i);
                flag = 1;
                break;
            }
        }
        if (flag)
            break;
    }
    int flag = utils::rmfile(filename.data());
    if (flag != 0) {
        std::cout << "delete fail!" << std::endl;
        std::cout << strerror(errno) << std::endl;
    }
}

void KVStore::addsstable(sstable ss, int level) {
    sstableIndex[level].push_back(ss.getHead());
}

char strBuf[2097152];

/**
 * @brief Fetches a substring from a file starting at a given offset.
 *
 * This function opens a file in binary read mode, seeks to the specified start offset,
 * reads a specified number of bytes into a buffer, and returns the buffer as a string.
 *
 * @param file The path to the file from which to read the substring.
 * @param startOffset The offset in the file from which to start reading.
 * @param len The number of bytes to read from the file.
 * @return A string containing the read bytes.
 */
std::string KVStore::fetchString(std::string file, int startOffset, uint32_t len) {
    // TODO here
    // 1. 以二进制读模式打开文件
    std::ifstream inFile(file, std::ios::binary);
    if (!inFile.is_open()) {
        throw std::runtime_error("Failed to open file: " + file);
    }

    // 2. 检查 startOffset 是否有效，并移动文件指针
    if (startOffset < 0) {
        inFile.close();
        throw std::invalid_argument("Start offset cannot be negative");
    }

    // 移动到指定偏移量
    inFile.seekg(startOffset, std::ios::beg);
    if (!inFile) {
        inFile.close();
        throw std::runtime_error("Failed to seek to offset " + std::to_string(startOffset));
    }

    // 3. 创建缓冲区并读取指定长度的字节
    std::string buffer(len, '\0'); // 预分配 len 大小的字符串，初始化为 \0
    inFile.read(&buffer[0], len);

    // 检查实际读取的字节数
    std::streamsize bytesRead = inFile.gcount();
    if (bytesRead < 0 || static_cast<uint32_t>(bytesRead) < len) {
        // 如果读取的字节少于请求的长度，调整字符串大小
        buffer.resize(bytesRead);
    }

    // 4. 关闭文件并返回结果
    inFile.close();
    return buffer;
}



// 使用堆排序
std::vector<std::pair<std::uint64_t, std::string>> KVStore::search_knn(std::string query, int k){
    // 计算查询向量
    std::vector<float> queryVec = getEmbd(query);

    //维护小顶堆
    auto cmp = [](const std::pair<float, uint64_t>& a, const std::pair<float, uint64_t>& b) {
        return a.first > b.first; // 小顶堆，比较float值
    };
    std::priority_queue<std::pair<float, uint64_t>, std::vector<std::pair<float, uint64_t>>, decltype(cmp)> minHeap(cmp);


    // 遍历所有嵌入向量（embeddings映射和磁盘文件），计算相似度
    load_embedding_from_disk(); // 为遍历，暂时将所有向量加载回内存
    for (auto it: embeddings) {
        // 若该key已被删除，则跳过
        if (get(it.first) == "")continue;
        float sim = common_embd_similarity_cos(queryVec.data(), it.second.data(), vec_dim);
        minHeap.push(std::make_pair(sim, it.first));

        // 若堆的大小超过了k，则弹出最小的元素
        while (minHeap.size() > k) minHeap.pop();
    }

    //将小顶堆中的元素存入向量，每次存入开头部分，以达到降序排列
    std::vector<std::pair<uint64_t, std::string>> result;
    while (!minHeap.empty()){
        result.insert(result.begin(), std::make_pair(minHeap.top().second, get(minHeap.top().second)));
        minHeap.pop();
    }

    save_embedding_to_disk(); // 遍历完后，将向量存回
    return result;
}


std::vector<std::pair<std::uint64_t, std::string>> KVStore::search_knn_hnsw(std::string query, int k){
    // 计算查询向量
    std::vector<float> queryVec = getEmbd(query);
    std::vector<uint64_t> result_key = hnswIndex->search_knn_hnsw(queryVec, k);
    // 将结果键值对存入向量
    std::vector<std::pair<std::uint64_t, std::string>> result;
    for (auto &it: result_key) {
        result.push_back(std::make_pair(it, get(it)));
    }
    return result;
}

/**
 * @brief 将KVStore中embeddings中存储的键-嵌入对写入磁盘，并清空embeddings
 * @param filename 文件路径
 */
void KVStore::save_embedding_to_disk(const std::string &filename) {
    // 打开文件，使用二进制追加模式
    std::ofstream outfile(filename, std::ios::binary | std::ios::app);

    // 检查文件是否成功打开
    if (!outfile.is_open()) {
        std::cerr << "无法打开文件进行追加写入: " << filename << std::endl;
        return;
    }

    for (auto it: embeddings) {
        // 先写入8B的key
        outfile.write((const char *)(&(it.first)), sizeof(uint64_t));
        // 再写入768*8B的embedding
        for (size_t i = 0; i < it.second.size(); ++i) {
            outfile.write((const char *)(it.second.data() + i), sizeof(float));
        }
    }

    // 关闭文件
    outfile.close();

    // 清空内存中的embeddings
    embeddings.clear();
}

std::vector<float> KVStore::search_embedding(uint64_t key, const std::string &filename) {
    if (embeddings.contains(key)) return embeddings[key];// 可能是删除标记，也可能是嵌入向量
    else {
        // 在文件中查找
        std::ifstream infile(filename, std::ios::binary);
        if (!infile) {
            return {}; // 文件打开失败，返回空向量
        }

        // 获取文件大小
        infile.seekg(0, std::ios::end);
        std::streampos fileSize = infile.tellg();

        // 每个key-vector对的大小
        const size_t keySize = sizeof(uint64_t);
        const size_t vectorSize = 768 * sizeof(float);
        const size_t recordSize = keySize + vectorSize;

        // 从文件末尾开始向前搜索
        std::streampos pos = fileSize;
        while (pos >= static_cast<std::streampos>(recordSize)) {
            pos -= recordSize;
            infile.seekg(pos, std::ios::beg);

            // 读取key
            uint64_t currentKey;
            infile.read(reinterpret_cast<char*>(&currentKey), keySize);

            if (infile.fail()) {
                break; // 读取失败，退出循环
            }

            // 如果找到匹配的key
            if (currentKey == key) {
                // 读取对应的向量
                std::vector<float> vector(768);
                infile.read(reinterpret_cast<char*>(vector.data()), vectorSize);

                if (!infile.fail()) {
                    return vector; // 成功找到并返回向量
                }
                break; // 读取向量失败
            }
        }

        return {}; // 未找到，返回空向量
    }
}

/**
 * @brief 系统启动时，从磁盘加载嵌入向量，放置到embeddings中
 * @param data_root 存放键-嵌入对的文件路径
 */
void KVStore::load_embedding_from_disk(const std::string &data_root) {
    // 打开文件，使用二进制读模式
    std::ifstream infile(data_root, std::ios::binary);
    // 检查文件是否成功打开
    if (!infile.is_open()) {
        std::cerr << "无法打开文件进行读取: " << data_root << std::endl;
        return;
    }
    // 读取嵌入向量的维度
    uint64_t dimension;
    infile.read((char *)&dimension, sizeof(uint64_t));
    if (dimension != 768) {
        std::cerr << "嵌入向量维度不匹配: " << dimension << std::endl;
        infile.close();
        return;
    }
    // 读取键-嵌入对
    while (infile.peek() != EOF) {
        uint64_t key;
        infile.read((char *)&key, sizeof(uint64_t));
        std::vector<float> embedding(dimension);
        infile.read((char *)embedding.data(), dimension * sizeof(float));
        embeddings[key] = embedding;
    }
    // 关闭文件
    infile.close();
}

/**
 * @brief 将HNSW索引保存到磁盘
 * @param hnsw_data_root HNSW保存的路径根目录
*/
void KVStore::save_hnsw_index_to_disk(const std::string &hnsw_data_root) {
    if (!hnswIndex) return;
    hnswIndex->saveToDisk(hnsw_data_root);
}



/**
* @brief 从磁盘加载HNSW索引
* @param hnsw_data_root HNSW保存的路径根目录，带"/"
*/
void KVStore::load_hnsw_index_from_disk(const std::string &hnsw_data_root) {
    if (hnswIndex) {
        delete hnswIndex;
        hnswIndex = nullptr;
    }
    // 先读取全局参数文件
    std::string global_header_filename = hnsw_data_root + "global_header.bin";
    std::ifstream global_header_file(global_header_filename, std::ios::binary);
    if (!global_header_file.is_open()) {
        // 打开失败
        std::cerr << "无法打开文件进行读取: " << global_header_filename << std::endl;
        return;
    }
    // 读取HNSW参数
    uint32_t M, M_max, efConstruction, m_L, max_level, num_nodes, dim;
    global_header_file.read((char *)&M, sizeof(uint32_t));
    global_header_file.read((char *)&M_max, sizeof(uint32_t));
    global_header_file.read((char *)&efConstruction, sizeof(uint32_t));
    global_header_file.read((char *)&m_L, sizeof(uint32_t));
    global_header_file.read((char *)&max_level, sizeof(uint32_t));
    global_header_file.read((char *)&num_nodes, sizeof(uint32_t));
    global_header_file.read((char *)&dim, sizeof(uint32_t));
    // 读取完毕，关闭文件
    global_header_file.close();


    if (num_nodes == 0) {
        if (hnswIndex) delete hnswIndex;
        hnswIndex = new HNSWIndex(M, M_max, efConstruction, nullptr, m_L);
        return;// HNSW中没有结点
    }
    // 读取所有结点的数据，建立离散结点，并维护id至结点的映射
    std::unordered_map<uint32_t, Node*> map;
    for (uint32_t index = 0; index < num_nodes; index++) {
        // 读取header.bin文件
        std::string header_filename = hnsw_data_root + "nodes/" + std::to_string(index) + "/header.bin";
        std::ifstream header_file(header_filename, std::ios::binary);
        if (!header_file.is_open()) {
            // 打开失败
            std::cerr << "无法打开文件进行读取: " << header_filename << std::endl;
            return;
        }
        // 读取结点的层数和key
        uint32_t level;
        uint64_t key_of_embedding_vector;
        header_file.read((char *)&level, sizeof(uint32_t));
        header_file.read((char *)&key_of_embedding_vector, sizeof(uint64_t));
        // 关闭文件
        header_file.close();
        // 创建结点并建立映射
        std::vector<float> vec;

        // 先尝试从embeddings中寻找嵌入向量
        vec = search_embedding(key_of_embedding_vector); // 可能是某个嵌入向量，也可能是删除标记，也可能为空
        Node *cur = new Node(level, key_of_embedding_vector, vec);
        map[index] = cur;
    }

    // 0号结点即为该HNSWIndex的entry结点
    // 设置HNSW参数
    hnswIndex = new HNSWIndex(M, M_max, efConstruction, map[0], m_L);
    // 剩下三个参数：max_level, num_nodes, dim

    // 根据邻接信息连接所有邻居
    for (uint64_t index = 0; index < num_nodes; index++) {
        std::string node_edge_dir = hnsw_data_root + "nodes/" + std::to_string(index) + "/edges/";
        // 分层进行连接
        for (uint32_t l = 0; l < map[index]->level; l++) {
            // 读取邻接信息
            std::string neighbor_filename = node_edge_dir + std::to_string(l) + ".bin";
            std::ifstream neighbor_file(neighbor_filename, std::ios::binary);
            if (!neighbor_file.is_open()) {
                // 打开失败
                std::cerr << "无法打开文件进行读取: " << neighbor_filename << std::endl;
                return;
            }
            // 先读取该层邻居个数
            uint32_t neighborNum;
            neighbor_file.read((char *)&neighborNum, sizeof(uint32_t));
            for (uint32_t i = 0; i < neighborNum; i++) {
                // 读取邻居id
                uint32_t neighbor_id;
                neighbor_file.read((char *)&neighbor_id, sizeof(uint32_t));
                // 将邻居连接到当前结点
                map[index]->neighbors[l].push_back(map[neighbor_id]);
            }
        }
    }

    // 上述操作结束后，被删除的结点被赋予了错误的值，需要根据deleted_nodes.bin文件的内容恢复
    std::string deleted_nodes_filename = hnsw_data_root + "deleted_nodes.bin";
    std::ifstream deleted_nodes_file(deleted_nodes_filename, std::ios::binary);
    if (!deleted_nodes_file.is_open()) {
        // 打开失败
        std::cerr << "无法打开文件进行读取: " << deleted_nodes_filename << std::endl;
        return;
    }
    // 读取被删除结点的个数
    uint32_t deleted_nodes_num;
    deleted_nodes_file.read((char *)&deleted_nodes_num, sizeof(uint32_t));
    for (uint32_t index = 0; index < deleted_nodes_num; index++) {
        // 读取被删除结点的id
        uint32_t deleted_node_id;
        deleted_nodes_file.read((char *)&deleted_node_id, sizeof(uint32_t));
        // 读取被删除结点的向量数据
        std::vector<float> deleted_node_vec(vec_dim);
        deleted_nodes_file.read((char *)deleted_node_vec.data(), vec_dim * sizeof(float));
        // 将被删结点的向量设为读出的向量值
        map[deleted_node_id]->embedding = deleted_node_vec;
        // 将该结点加入HNSW索引的删除集合
        hnswIndex->del(map[deleted_node_id]->key, deleted_node_vec);
    }
}

std::vector<float> KVStore::getEmbd(std::string str) {
    // 先尝试在ref文件中查找
    std::vector<float> vec = util.getVec(str);
    if (vec.size() == 0) vec = embedding(str)[0];
    return vec;
}

std::vector<std::pair<std::uint64_t, std::string>> KVStore::search_knn_hnsw_parallel(std::string query, int k){
    // 计算查询向量
    std::vector<float> queryVec = getEmbd(query);
    std::vector<uint64_t> result_key = hnswIndex->search_knn_hnsw_parallel(queryVec, k);
    // 将结果键值对存入向量
    std::vector<std::pair<std::uint64_t, std::string>> result;
    for (auto &it: result_key) {
        result.push_back(std::make_pair(it, get(it)));
    }
    return result;
}