cmake_minimum_required(VERSION 3.21)

# initially
# > mkdir cmake
# > wget https://raw.githubusercontent.com/cpp-pm/gate/master/cmake/HunterGate.cmake -O cmake/HunterGate.cmake
# include("cmake/HunterGate.cmake")
# HunterGate(
#   URL "https://github.com/cpp-pm/hunter/archive/v0.23.297.tar.gz"
#   SHA1 "3319fe6a3b08090df7df98dee75134d68e2ef5a3"
# )

# set(HUNTER_ENABLED 0)

project(testListenToOneConsensus VERSION 1.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


set(Boost_USE_STATIC_LIBS ON)
set(Boost_DIR "/home/me/.local/boost_1_82_0/stage/lib/cmake/Boost-1.82.0/")
find_package(Boost 1.75...1.82
  CONFIG REQUIRED COMPONENTS json log
)

add_executable(main pure-testListenToOne.cpp)
target_include_directories(main PUBLIC ..)

target_link_libraries(main PUBLIC Boost::log Boost::json)

#[=[

cd ../../..
cmake -S weak/include/cnsss/test-listenToOne -B build-testListenToOneConsensus
cmake --build build-testListenToOneConsensus
./build-testListenToOneConsensus/main
#]=]
