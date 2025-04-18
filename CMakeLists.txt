cmake_minimum_required(VERSION 3.10)

#project(LSM_TREE)
project(LSM_TREE CXX CUDA)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


option(GGML_CUDA "Enable CUDA in ggml" ON)
set(GGML_CUDA_GRAPHS_DEFAULT ON)

if(GGML_CUDA)
    message(STATUS "Using CUDA in ggml")
endif()



add_executable(correctness correctness.cc kvstore_api.h kvstore.h
        kvstore.cc skiplist.cpp skiplist.h sstable.cpp sstable.h
        bloom.cpp bloom.h MurmurHash3.h utils.h test.h
        sstablehead.cpp sstablehead.h
        HNSW.h
        HNSW.cpp)

add_executable(persistence persistence.cc kvstore_api.h kvstore.h kvstore.cc
        skiplist.cpp skiplist.h sstable.cpp sstable.h
        bloom.cpp bloom.h MurmurHash3.h utils.h test.h
        sstablehead.cpp sstablehead.h
        HNSW.h
        HNSW.cpp)


add_subdirectory(third_party/llama.cpp)

add_subdirectory(embedding)

add_subdirectory(test)

target_link_libraries(correctness PUBLIC embedding)
target_link_libraries(persistence PUBLIC embedding)

