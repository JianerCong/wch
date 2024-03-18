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

project(hi VERSION 1.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 🦜 : Enable the following
option(WITH_TEST "build test" ON)
option(WITH_PROTOBUF "build protobuf" ON)
option(WITH_PYTHON "build with python" ON)

include("cmake/deps.cmake")


add_subdirectory(pkgs/intx-0.10.0 intx)        #brings in the intx
add_subdirectory(pkgs/ethash-1.0.1 ethash)        #brings in ethash
add_subdirectory(pkgs/evmweak)        #brings in the evmweak

add_library(core-deps INTERFACE)

target_link_libraries(core-deps INTERFACE
  evmc::evmc_cpp
  OpenSSL::Crypto
  # evmc::loader evmc::instructions evmc::tooling
  Boost::unit_test_framework Boost::log Boost::json Boost::program_options
  intx::intx ethash::keccak
  # hi_pb
)

if (WITH_PROTOBUF)
  target_link_libraries(core-deps INTERFACE hi_pb)
  # define WITH_PROTOBUF
  target_compile_definitions(core-deps INTERFACE WITH_PROTOBUF)
endif()

add_library(deps INTERFACE)     #the ALL dependency target = core + rdb (currently)
target_link_libraries(deps INTERFACE core-deps RocksDB::rocksdb
  evmweak
)

if (WITH_PYTHON)
  file(READ other/verifier.py PY_VERIFIER_CONTENT)
  file(READ other/use-verifier.py PY_USE_VERIFY_CONTENT)
  file(READ other/invoke-template.py PY_INVOKE_TEMPLATE)
  # file(READ other/verifier.py PY_VERIFIER_CONTENT_HEX HEX)

  # 2. replace \n with \\n
  string(REPLACE "\n" "\\n" PY_VERIFIER_CONTENT "${PY_VERIFIER_CONTENT}")
  string(REPLACE "\n" "\\n" PY_USE_VERIFY_CONTENT "${PY_USE_VERIFY_CONTENT}")
  string(REPLACE "\n" "\\n" PY_INVOKE_TEMPLATE "${PY_INVOKE_TEMPLATE}")

  configure_file(include/config.hpp.in config.hpp ESCAPE_QUOTES)
  target_include_directories(deps INTERFACE ${CMAKE_CURRENT_BINARY_DIR})
endif()

target_include_directories(deps INTERFACE
  ${PROJECT_SOURCE_DIR}/pkgs/
)


if(WITH_TEST)
  add_subdirectory(test)
endif()

add_executable(wch main.cpp)

if (WITH_PROTOBUF)
  # 🦜 : Add that definitions
  target_compile_definitions(wch PUBLIC WITH_PROTOBUF)
  target_link_libraries(wch PUBLIC hi_pb)
  target_link_libraries(deps INTERFACE protobuf::libprotobuf)
endif()

target_include_directories(wch PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(wch PUBLIC deps)

# Copy these tests
# configure_file(helpers.py helpers.py)
# configure_file(test_with_mocked_exe.py test_with_mocked_exe.py)
# configure_file(test_with_normal_exe.py test_with_normal_exe.py)
# configure_file(test_with_light_exe.py test_with_light_exe.py)
# configure_file(test_psstn.py test_psstn.py)

# 🦜 : Make the
# the core's pb for python
set(o "${CMAKE_CURRENT_SOURCE_DIR}")
protobuf_generate(LANGUAGE python
  TARGET hi_pb_core
  OUT_VAR HI_PB_SRC_PY
  IMPORT_DIRS "${o0}/"
  PROTOC_OUT_DIR ${o}
  PROTOC_OPTIONS "--pyi_out=${o}"
)
file(GLOB l ${CMAKE_CURRENT_SOURCE_DIR}/*.py)
message(STATUS "got OOthon files: ${l}")
foreach(X IN LISTS l)
  file(RELATIVE_PATH x ${CMAKE_CURRENT_SOURCE_DIR} ${X}) #get the file name
  message(STATUS "🐸 Copying ${X} to ${CMAKE_CURRENT_BINARY_DIR}/${x}")
  configure_file(${X} ${CMAKE_CURRENT_BINARY_DIR}/${x})
endforeach()

# add_custom_target(run ALL pytest -v
#   -s                            #capture=no
#   COMMENT "Runing pytest 🐸")

## run the app upon build
# add_custom_target(run ALL main COMMENT "Runing App 🐸")

# rm -rf build
# cmake -S weak -B build -DCMAKE_BUILD_TYPE=Release
# cmake --build build --config Release
# cmake --build build --target run-test
