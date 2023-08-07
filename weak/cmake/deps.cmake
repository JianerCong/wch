
# 🦜 : USE_STATIC_LIBS is important, because by default, the loader cannot load
# dynamic library from non-default locations.
set(Boost_USE_STATIC_LIBS ON)


# try to find Boost on system
find_package(Boost 1.75...1.82 COMPONENTS json unit_test_framework log program_options)

if (Boost_FOUND)
  message("🐸 在本地找到了Boost")
else()
  set(Boost_DIR "${PROJECT_SOURCE_DIR}/../.pre/boost_1_82_0/stage/lib/cmake/Boost-1.82.0/")
  find_package(Boost 1.75...1.82
    CONFIG REQUIRED COMPONENTS json unit_test_framework log program_options
  )
endif()


set(RocksDB_DIR "${PROJECT_SOURCE_DIR}/../.pre/installed-rocksdb/lib/x86_64-linux-gnu/cmake/rocksdb")
# 🦜 : It seems a bug that we need to include this Finduring manually
include(${RocksDB_DIR}/modules/Finduring.cmake)
find_package(RocksDB CONFIG REQUIRED)


# include(CMakePrintHelpers)
# cmake_print_properties(TARGETS Boost::log PROPERTIES
#   TYPE SOURCE_DIR
#   LINK_DIRECTORIES
#   LOCATION
#   NAME
# )

# cmake_print_properties(TARGETS Boost::thread PROPERTIES
#   TYPE SOURCE_DIR
#   LINK_DIRECTORIES
#   LINK_LIBRARIES
#   LOCATION
#   NAME
# )
