cmake_minimum_required(VERSION 3.15)
project(BoltKV VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_library(storage_engine src/storage/storage_engine.cpp)

add_library(network_server src/network/tcp_server.cpp)

target_link_libraries(network_server storage_engine)

add_executable(BoltKV_db src/main.cpp)

target_link_libraries(BoltKV_db storage_engine network_server)

enable_testing()
add_subdirectory(tests)