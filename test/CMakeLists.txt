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



target_link_libraries(Embedding_Test PUBLIC embedding)

target_link_libraries(E2E_Test PUBLIC embedding)



# HNSW删除测试
add_executable(HNSW_Delete_Test
        HNSW_Delete_Test.cpp
        ../kvstore.cc
        ../skiplist.cpp
        ../sstable.cpp
        ../bloom.cpp
        ../sstablehead.cpp
        ../utils.h
        ../HNSW.h
        ../HNSW.cpp
)

target_compile_options(HNSW_Delete_Test PRIVATE
        -g -O0
)

target_link_libraries(HNSW_Delete_Test PUBLIC embedding)


# 简单跨平台测试
add_executable(Simple_Cross_Plat_Test
        Simple_Cross_Plat_Test.cpp
)

target_compile_options(Simple_Cross_Plat_Test PRIVATE
        -g -O0
)

target_link_libraries(Simple_Cross_Plat_Test PUBLIC embedding)

# 向量持久化测试
add_executable(Vector_Persistent_Test_Phase1
        Vector_Persistent_Test_Phase1.cpp
        ../kvstore.cc
        ../skiplist.cpp
        ../sstable.cpp
        ../bloom.cpp
        ../sstablehead.cpp
        ../utils.h
        ../HNSW.h
        ../HNSW.cpp
)

# 为测试设置调试编译选项
target_compile_options(Vector_Persistent_Test_Phase1 PRIVATE
        -g -O0
)

target_link_libraries(Vector_Persistent_Test_Phase1 PUBLIC embedding)


# 向量持久化测试
add_executable(Vector_Persistent_Test_Phase2
        Vector_Persistent_Test_Phase2.cpp
        ../kvstore.cc
        ../skiplist.cpp
        ../sstable.cpp
        ../bloom.cpp
        ../sstablehead.cpp
        ../utils.h
        ../HNSW.h
        ../HNSW.cpp
)

# 为测试设置调试编译选项
target_compile_options(Vector_Persistent_Test_Phase2 PRIVATE
        -g -O0
)

target_link_libraries(Vector_Persistent_Test_Phase2 PUBLIC embedding)



# HNSW持久化测试
add_executable(HNSW_Persistent_Test_Phase1
        HNSW_Persistent_Test_Phase1.cpp
        ../kvstore.cc
        ../skiplist.cpp
        ../sstable.cpp
        ../bloom.cpp
        ../sstablehead.cpp
        ../utils.h
        ../HNSW.h
        ../HNSW.cpp
)

# 为测试设置调试编译选项
target_compile_options(HNSW_Persistent_Test_Phase1 PRIVATE
        -g -O0
)

target_link_libraries(HNSW_Persistent_Test_Phase1 PUBLIC embedding)

add_executable(HNSW_Persistent_Test_Phase2
        HNSW_Persistent_Test_Phase2.cpp
        ../kvstore.cc
        ../skiplist.cpp
        ../sstable.cpp
        ../bloom.cpp
        ../sstablehead.cpp
        ../utils.h
        ../HNSW.h
        ../HNSW.cpp
)

# 为测试设置调试编译选项
target_compile_options(HNSW_Persistent_Test_Phase2 PRIVATE
        -g -O0
)

target_link_libraries(HNSW_Persistent_Test_Phase2 PUBLIC embedding)



# my test
add_executable(My_Test
        My_Test.cpp
        ../kvstore.cc
        ../skiplist.cpp
        ../sstable.cpp
        ../bloom.cpp
        ../sstablehead.cpp
        ../utils.h
        ../HNSW.h
        ../HNSW.cpp
        ../test.h
)

# 为测试设置调试编译选项
target_compile_options(My_Test PRIVATE
        -g -O0
)

target_link_libraries(My_Test PUBLIC embedding)


# HNSW持久化测试
add_executable(HNSW_Basic_Persistent_Test_Phase1
        HNSW_Basic_Persistent_Test_Phase1.cpp
        ../kvstore.cc
        ../skiplist.cpp
        ../sstable.cpp
        ../bloom.cpp
        ../sstablehead.cpp
        ../utils.h
        ../HNSW.h
        ../HNSW.cpp
)

# 为测试设置调试编译选项
target_compile_options(HNSW_Basic_Persistent_Test_Phase1 PRIVATE
        -g -O0
)

target_link_libraries(HNSW_Basic_Persistent_Test_Phase1 PUBLIC embedding)


add_executable(HNSW_Basic_Persistent_Test_Phase2
        HNSW_Basic_Persistent_Test_Phase2.cpp
        ../kvstore.cc
        ../skiplist.cpp
        ../sstable.cpp
        ../bloom.cpp
        ../sstablehead.cpp
        ../utils.h
        ../HNSW.h
        ../HNSW.cpp
)

# 为测试设置调试编译选项
target_compile_options(HNSW_Basic_Persistent_Test_Phase2 PRIVATE
        -g -O0
)

target_link_libraries(HNSW_Basic_Persistent_Test_Phase2 PUBLIC embedding)


