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


class E2ETest_Eval : public Test {
private:
    void prepare(int max) {
        store.reset();
        std::cout << "start preparing data" << std::endl;
        for(int idx = 0; idx < max; idx++) store.put(idx, util.getStr(idx));
        std::cout << "finish preparing data" << std::endl;
    }

    void text_test(int max){
        int idx;
        // test search_knn_hnsw function
        std::cout << "Testing search_knn_hnsw function:" << std::endl;
        int k = 3, passCnt = 0;

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

        std::cout << "test set size: " << max << std::endl;
        std::cout << "k: " << k << std::endl;
        std::cout << "search_knn_hnsw pass count: " << passCnt << std::endl;
        std::cout << "search_knn_hnsw accuracy: " << ((passCnt + 0.0) / max) << std::endl;
        
    }




public:
    E2ETest_Eval(const std::string &dir, bool v = true) : Test(dir, v) {}

    void start_test(void *args = NULL) override {

        prepare(500);
        text_test(500);

        // prepare(32768);
        // for (int i = 0; i < 4; i++){
        //     std::cout << "[Text Test" << i <<" ]" << std::endl;
        //     text_test(4096 * (1 << i)); // 4096, 8192, 16384, 32768
        // }
    }
};

int main(int argc, char *argv[]) {
    bool verbose = (argc == 2 && std::string(argv[1]) == "-v");

    std::cout << "Usage: " << argv[0] << " [-v]" << std::endl;
    std::cout << "  -v: print extra info for failed tests [currently ";
    std::cout << (verbose ? "ON" : "OFF") << "]" << std::endl;
    std::cout << std::endl;
    std::cout.flush();

    E2ETest_Eval test("./data", verbose);

    test.start_test();

    return 0;
}
