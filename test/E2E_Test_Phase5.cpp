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

        // 测试串行search_knn_hnsw - 添加计时
        auto start_time = std::chrono::high_resolution_clock::now();

        for(idx = 0; idx < max; ++idx) {
            auto res = store.search_knn_hnsw(util.getStr(idx), k);
            for (int i = 0; i < k; i++) {
                if (res[i].second == util.getStr(idx)) {passCnt++;break;}
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> serial_duration = end_time - start_time;
        double avg_serial_time = serial_duration.count() / max;

        std::cout << "test set size: " << max << std::endl;
        std::cout << "k: " << k << std::endl;
        std::cout << "search_knn_hnsw pass count: " << passCnt << std::endl;
        std::cout << "search_knn_hnsw accuracy: " << ((passCnt + 0.0) / max) << std::endl;
        std::cout << "search_knn_hnsw total time: " << serial_duration.count() << " ms" << std::endl;
        std::cout << "search_knn_hnsw average time: " << avg_serial_time << " ms/query" << std::endl;

        // 测试并行search_knn_hnsw_parallel - 添加计时
        start_time = std::chrono::high_resolution_clock::now();

        for(idx = 0; idx < max; ++idx) {
            auto res = store.search_knn_hnsw_parallel(util.getStr(idx), k);
            for (int i = 0; i < k; i++) {
                if (res[i].second == util.getStr(idx)) {passCnt_parallel++;break;}
            }
        }

        end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> parallel_duration = end_time - start_time;
        double avg_parallel_time = parallel_duration.count() / max;

        std::cout << "test set size: " << max << std::endl;
        std::cout << "k: " << k << std::endl;
        std::cout << "search_knn_hnsw_parallel pass count: " << passCnt_parallel << std::endl;
        std::cout << "search_knn_hnsw_parallel accuracy: " << ((passCnt_parallel + 0.0) / max) << std::endl;
        std::cout << "search_knn_hnsw_parallel total time: " << parallel_duration.count() << " ms" << std::endl;
        std::cout << "search_knn_hnsw_parallel average time: " << avg_parallel_time << " ms/query" << std::endl;
        std::cout << "加速比: " << (serial_duration.count() / parallel_duration.count()) << "x" << std::endl;
    }










public:
    E2ETest(const std::string &dir, bool v = true) : Test(dir, v) {util.init();}

    void start_test(void *args = NULL) override {
        std::cout << "===========================" << std::endl;
        std::cout << "KVStore Correctness Test" << std::endl;

        store.reset();
        std::cout << "[Text Test]" << std::endl;
        text_test(400); // 指定测试行数，原为120
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
