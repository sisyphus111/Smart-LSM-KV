#include "../kvstore.h"
#include "../util.h"
#include <iostream>


int main() {

    std::string testStr = util.getStr(27);
    std::vector<float> testVec = util.getVec(27);
    std::vector<float> testVec2 = util.getVec(testStr);
    std::vector<float> testVec3 = embedding(testStr)[0];

    std::string testStr2 = util.getStr(40050);
    std::vector<float> testVec4 = util.getVec(40050);
    std::vector<float> testVec5 = util.getVec(testStr2);
    std::vector<float> testVec6 = embedding(testStr2)[0];

    std::cout << common_embd_similarity_cos(testVec.data(), testVec3.data(), 768) << std::endl;
    std::cout << common_embd_similarity_cos(testVec4.data(), testVec6.data(), 768) << std::endl;
    return 0;
}

