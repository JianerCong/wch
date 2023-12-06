# 🦜 : USE_STATIC_LIBS is important, because by default, the loader cannot load
# dynamic library from non-default locations.
set(Boost_USE_STATIC_LIBS ON)


# try to find Boost on system
find_package(Boost 1.82 COMPONENTS json unit_test_framework log program_options)

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

if (WITH_PROTOBUF)
  set(x "${PROJECT_SOURCE_DIR}/../.pre/installed-pb")
  set(utf8_range_DIR "${x}/lib/cmake/utf8_range")
  set(absl_DIR "${x}/lib/cmake/absl")
  set(Protobuf_DIR "${x}/lib/cmake/protobuf")
  find_package(Protobuf CONFIG REQUIRED)

  # 🦜 : Here we generate the required pb files
  # --------------------------------------------------
  # 1. the proto for the cnsss
  set(o0 "${PROJECT_SOURCE_DIR}/include")
  add_library(hi_pb
    "${o0}/cnsss/pure-rbft.proto"
    "${o0}/cnsss/pure-listenToOne.proto"
  )
  set(o "${o0}/.generated_pb")
  # mkdir first
  file(MAKE_DIRECTORY "${o}")

  protobuf_generate(LANGUAGE cpp
    TARGET hi_pb
    OUT_VAR HI_PB_SRC
    IMPORT_DIRS "${o0}/cnsss/"
    # A common parent folder for the schema files. For example, if:
    #
    # schema file     = proto/hi/hi.proto
    # import folder   = proto/
    # generated files = ${PROTOC_OUT_DIR}/hi/hi.pb.h|cc
    PROTOC_OUT_DIR "${o}"
  )
  target_link_libraries(hi_pb PUBLIC protobuf::libprotobuf)
  message("🐸 HI_PB_SRC: ${HI_PB_SRC}")
endif()

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
