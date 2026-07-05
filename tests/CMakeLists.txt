cmake_minimum_required(VERSION 3.15)

add_executable(storage_test storage_test.cpp)

target_link_libraries(storage_test storage_engine)

add_test(NAME StorageEngineTest COMMAND storage_test)