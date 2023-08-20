cmake_minimum_required(VERSION 3.21)


project(testRbft VERSION 1.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


set(Boost_USE_STATIC_LIBS ON)
set(Boost_DIR "/home/me/.local/boost_1_82_0/stage/lib/cmake/Boost-1.82.0/")
find_package(Boost 1.75...1.82
  CONFIG REQUIRED COMPONENTS json log
)

add_executable(main pure-testRbft.cpp)
target_include_directories(main PUBLIC ..)

target_link_libraries(main PUBLIC Boost::log Boost::json)

#[=[

cd ../../..

. ./s-options.sh ./weak/include/cnsss/test-rbdt ./weak
cmake -S weak/include/cnsss/test-rbft -B build-testRbft
cmake --build build-hi
./build-hi/main 2
./build-hi/main 3
#]=]
