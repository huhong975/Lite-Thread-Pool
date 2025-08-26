cmake_minimum_required(VERSION 3.16)
project(ThreadPool)

include_directories(
    include
)
set(CMAKE_CXX_STANDARD 17)
add_executable(threadpool src/main.cpp src/threadpool.cpp)