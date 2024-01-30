# 🦜 : USE_STATIC_LIBS is important, because by default, the loader cannot load
# dynamic library from non-default locations.
set(Boost_USE_STATIC_LIBS ON)

# Boost_VERBOSE:            Enable verbose output
# Boost_DEBUG:              Enable debug (even more verbose) output

if(WIN32)
  add_compile_options(/utf-8 /bigobj)
  add_definitions(-D_WIN32_WINNT=0x0A00) #minimum windows 10
  set(x ${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg/installed/x64-windows-static/)
  include_directories(${x}/include)
  set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded) #🦜 : This must match the one we linked against to...
  #[=[
  Select the MSVC runtime library for use by compilers targeting the MSVC ABI. it can be
  1. MultiThreaded
  2. MultiThreadedDebug
  3. MultiThreadedDLL
  4. MultiThreadedDebugDLL
  #]=]

  set(Boost_USE_STATIC_RUNTIME ON)
  # set(Boost_VERBOSE ON)
  set(Boost_DEBUG ON)

  set(Boost_USE_DEBUG_LIBS OFF)        # Use release libraries
  set(Boost_USE_DEBUG_RUNTIME OFF)        # Use release libraries
endif()

# try to find Boost on system
find_package(Boost 1.75...1.99 COMPONENTS json unit_test_framework log program_options)

if (Boost_FOUND)
  message("🐸 Found Boost locally")
else()
  if (WIN32)
    message("🐸 Finding Boost target on windows")
    set(Boost_DIR "${PROJECT_SOURCE_DIR}/../.pre/boost_1_84_0/stage/lib/cmake/Boost-1.84.0/")
  else()
    message("🐸 在本地没有找到Boost，我们将使用预编译的Boost")
    set(Boost_DIR "${PROJECT_SOURCE_DIR}/../.pre/boost_1_82_0/stage/lib/cmake/Boost-1.82.0/")
  endif()
  find_package(Boost 1.75...1.99
    CONFIG REQUIRED COMPONENTS json unit_test_framework log program_options
  )
endif()


if (WIN32)
  # manually import the libs
  message("🐸 manually making rocksdb on windows")
  set(x ${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg/installed/x64-windows-static/lib)
  add_library(RocksDB::rocksdb STATIC IMPORTED)
  set_target_properties(RocksDB::rocksdb PROPERTIES
    IMPORTED_LOCATION "${x}/rocksdb.lib"
  )
  target_link_libraries(RocksDB::rocksdb INTERFACE
    # "${x}/bz2.lib"
    "${x}/zlib.lib"
  )
else()
  set(RocksDB_DIR "${PROJECT_SOURCE_DIR}/../.pre/installed-rocksdb/lib/x86_64-linux-gnu/cmake/rocksdb")
  # 🦜 : It seems a bug that we need to include this Finduring manually
  include(${RocksDB_DIR}/modules/Finduring.cmake)
  find_package(RocksDB CONFIG REQUIRED)
endif()


if (WIN32)
  # manually import the libs
  message("🐸 manually making protobuf on windows")
  # set(x ${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg/installed/x64-windows-static/lib/)
  # add_library(protobuf::libprotobuf STATIC IMPORTED)
  # set_target_properties(protobuf::libprotobuf PROPERTIES
  #   IMPORTED_LOCATION "${x}/libprotobuf.lib"
  # )

  # set(x "${PROJECT_SOURCE_DIR}/../.pre/installed-pb")
  set(Protobuf_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg/installed/x64-windows-static/share/protobuf)
  set(Protobuf_PROTOC_EXECUTABLE ${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg/installed/x64-windows-static/tools/protobuf/protoc.exe)
else()
  set(x "${PROJECT_SOURCE_DIR}/../.pre/installed-pb")
  set(utf8_range_DIR "${x}/lib/cmake/utf8_range")
  set(absl_DIR "${x}/lib/cmake/absl")
  set(Protobuf_DIR "${x}/lib/cmake/protobuf")
endif()

find_package(Protobuf CONFIG REQUIRED)
# 🦜 : will this work ?
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
      #
      # 🦜 : So, the generated files are kinda in ${PROTOC_OUT_DIR} + <schama dir>
      # - <import dir>
      PROTOC_OUT_DIR "${o}"
    )
    target_link_libraries(hi_pb PUBLIC protobuf::libprotobuf)
    message("🐸 HI_PB_SRC: ${HI_PB_SRC}")

#The core-dependencies (These should be cross-platform). 🦜 : These are the dependencies needed for most tests
if (WIN32)
  # manually import the libcrypto
  message("🐸 manually making libcrypto on windows")
  set(x ${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg/installed/x64-windows-static/lib/)
  add_library(OpenSSL::Crypto STATIC IMPORTED)
  set_target_properties(OpenSSL::Crypto PROPERTIES
    IMPORTED_LOCATION "${x}/libcrypto.lib"
  )
else()
find_package(OpenSSL REQUIRED)
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
