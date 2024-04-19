cmake_minimum_required(VERSION 3.21)

project(testRbft VERSION 1.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_DIR "/home/me/.local/boost_1_82_0/stage/lib/cmake/Boost-1.82.0/")
find_package(Boost 1.75...1.82
  CONFIG REQUIRED COMPONENTS json log
)

add_executable(main pure-testRaft.cpp)
target_link_libraries(main PUBLIC Boost::log Boost::json)
target_compile_definitions(main PUBLIC WEAK_CNSSS_NO_CONFIG) #🦜 : No config.hpp

target_include_directories(main PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..)    #include/cnsss
target_include_directories(main PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../..) #include/
#[=[

cd ../../..
. ./s-options.sh ./weak/include/cnsss/test-raft ./weak
./build-hi/main 3

# 🦜 : Or manually build

cmake -S weak/include/cnsss/test-raft -B build-testRaft
cmake --build build-testRaft
./build-testRaft/main 2
./build-testRaft/main 3

N0 a
# kick N0
k 0
N1 b

#]=]
