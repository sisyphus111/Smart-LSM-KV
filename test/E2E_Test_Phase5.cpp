#include "../test.h"
#include <fstream>
#include <iostream>
#include <string>
#include <chrono>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <fstream>


class E2ETest : public Test {
private:
    const uint64_t SIMPLE_TEST_MAX = 512;
    const uint64_t MIDDLE_TEST_MAX  = 1024 * 64;
    const uint64_t LARGE_TEST_MAX  = 1024 * 64;

    repoUtil util;

    void text_test(int max){
        // test get function
        int idx = 0;
        max = std::min(max, 50000);
        for(idx = 0; idx < max; idx++) store.put(idx, util.getStr(idx));
        for(idx = 0; idx < max; idx++) EXPECT(util.getStr(idx), store.get(idx));
        phase();
        // test search_knn_hnsw function
        std::cout << "Testing search_knn_hnsw function:" << std::endl;
        int k = 3, passCnt = 0, passCnt_parallel = 0;

        // 测试串行search_knn_hnsw
        for(idx = 0; idx < max; ++idx) {
            auto res = store.search_knn_hnsw(util.getStr(idx), k);
            for (int i = 0; i < k; i++) {

                if (res[i].second == util.getStr(idx)) {passCnt++;break;}
            }
        }

        std::cout << "test set size: " << max << std::endl;
        std::cout << "k: " << k << std::endl;
        std::cout << "search_knn_hnsw pass count: " << passCnt << std::endl;
        std::cout << "search_knn_hnsw accuracy: " << float(passCnt / max) << std::endl;

        // 测试并行search_knn_hnsw
        for(idx = 0; idx < max; ++idx) {
            auto res = store.search_knn_hnsw_parallel(util.getStr(idx), k);
            for (int i = 0; i < k; i++) {

                if (res[i].second == util.getStr(idx)) {passCnt_parallel++;break;}
            }
        }

        std::cout << "test set size: " << max << std::endl;
        std::cout << "k: " << k << std::endl;
        std::cout << "search_knn_hnsw pass count: " << passCnt_parallel << std::endl;
        std::cout << "search_knn_hnsw accuracy: " << float(passCnt_parallel / max) << std::endl;
    }
public:
    E2ETest(const std::string &dir, bool v = true) : Test(dir, v) {util.init();}

    void start_test(void *args = NULL) override {
        std::cout << "===========================" << std::endl;
        std::cout << "KVStore Correctness Test" << std::endl;

        store.reset();
        std::cout << "[Text Test]" << std::endl;
        text_test(40000); // 指定测试行数，原为120
    }
};

int main(int argc, char *argv[]) {
    bool verbose = (argc == 2 && std::string(argv[1]) == "-v");

    std::cout << "Usage: " << argv[0] << " [-v]" << std::endl;
    std::cout << "  -v: print extra info for failed tests [currently ";
    std::cout << (verbose ? "ON" : "OFF") << "]" << std::endl;
    std::cout << std::endl;
    std::cout.flush();

    E2ETest test("./data", verbose);

    test.start_test();

    return 0;
}
