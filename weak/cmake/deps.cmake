
# 🦜 : USE_STATIC_LIBS is important, because by default, the loader cannot load
# dynamic library from non-default locations.
set(Boost_USE_STATIC_LIBS ON)
set(Boost_DIR "/home/me/.local/boost_1_82_0/stage/lib/cmake/Boost-1.82.0/")
find_package(Boost 1.75...1.82
  CONFIG REQUIRED COMPONENTS json unit_test_framework log program_options
)


# It seems a bug that we need to include this Finduring
include(/home/me/repo/installed-rocksdb/lib/x86_64-linux-gnu/cmake/rocksdb/modules/Finduring.cmake)
set(RocksDB_DIR "/home/me/repo/installed-rocksdb/lib/x86_64-linux-gnu/cmake/rocksdb")
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
