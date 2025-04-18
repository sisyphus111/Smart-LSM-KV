add_executable(Embedding_Test Embedding_Test.cpp
        ../HNSW.h
        ../HNSW.cpp)
add_executable(E2E_Test
    E2E_test.cpp
    ../kvstore.cc
    ../skiplist.cpp
    ../sstable.cpp
    ../bloom.cpp
    ../sstablehead.cpp
    ../utils.h
    ../HNSW.h
    ../HNSW.cpp
)
# 为 E2E_Test 设置调试编译选项
target_compile_options(E2E_Test PRIVATE
    -g -O0
)

target_link_libraries(Embedding_Test PUBLIC embedding)

target_link_libraries(E2E_Test PUBLIC embedding)
