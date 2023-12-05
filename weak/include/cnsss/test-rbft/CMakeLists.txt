cmake_minimum_required(VERSION 3.21)


project(testRbft VERSION 1.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


set(Boost_USE_STATIC_LIBS ON)
set(Boost_DIR "/home/me/.local/boost_1_82_0/stage/lib/cmake/Boost-1.82.0/")
find_package(Boost 1.75...1.82
  CONFIG REQUIRED COMPONENTS json log
)

option(ENABLE_PROTOBUF "Enable protobuf" ON)

if(ENABLE_PROTOBUF)
  set(x "/home/me/repo/installed-pb")
  set(utf8_range_DIR "${x}/lib/cmake/utf8_range")
  set(absl_DIR "${x}/lib/cmake/absl")
  set(Protobuf_DIR "${x}/lib/cmake/protobuf")
  find_package(Protobuf CONFIG REQUIRED)

  add_library(hi_pb "${CMAKE_CURRENT_LIST_DIR}/../pure-rbft.proto")

  set(o "${CMAKE_CURRENT_LIST_DIR}/../.generated_pb")
  # mkdir first
  file(MAKE_DIRECTORY "${o}")

  protobuf_generate(LANGUAGE cpp
    TARGET hi_pb
    OUT_VAR HI_PB_SRC
    IMPORT_DIRS "${CMAKE_CURRENT_LIST_DIR}/.."
    # A common parent folder for the schema files. For example, if:
    #
    # schema file     = proto/hi/hi.proto
    # import folder   = proto/
    # generated files = ${PROTOC_OUT_DIR}/hi/hi.pb.h|cc
    PROTOC_OUT_DIR "${o}"
  )
  target_link_libraries(hi_pb PUBLIC protobuf::libprotobuf)
  message("🐸 HI_PB_SRC: ${HI_PB_SRC}")

  add_executable(main pure-testRbft.cpp)

  target_compile_definitions(main PUBLIC WITH_PROTOBUF)
  target_link_libraries(main PUBLIC
    Boost::json
    Boost::log
    protobuf::libprotobuf
    hi_pb
  )


else()
  add_executable(main pure-testRbft.cpp)
  target_link_libraries(main PUBLIC Boost::log Boost::json)
endif()

target_include_directories(main PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..)    #include/cnsss
target_include_directories(main PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../..) #include/
#[=[

cd ../../..
. ./s-options.sh ./weak/include/cnsss/test-rbft ./weak
./build-hi/main 3

# 🦜 : Or manually build
cmake -S weak/include/cnsss/test-rbft -B build-testRbft
cmake --build build-testRbft
./build-testRbft/main 2
./build-testRbft/main 3

#]=]
