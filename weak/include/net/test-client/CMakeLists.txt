cmake_minimum_required(VERSION 3.10)

# set the project name and version
project(Hi VERSION 1.0)

IF (WIN32)
  # set stuff for windows
  set(Boost_DIR "C:\\Users\\congj\\repo\\boost_1_82_0\\stage\\lib\\cmake\\Boost-1.82.0")
  message("🐸 Setting Boost_DIR: ${Boost_DIR}")
else()
  # set stuff for other systems
  set(Boost_USE_STATIC_LIBS ON)
  set(Boost_DIR "/home/me/.local/boost_1_82_0/stage/lib/cmake/Boost-1.82.0/")
ENDIF()

find_package(Boost CONFIG REQUIRED
  unit_test_framework log locale log_setup json)
# add the executable

add_executable(myexe test_weakHttpClient.cpp)
add_executable(tps test_greenClient.cpp)

target_link_libraries(tps PUBLIC
  Boost::unit_test_framework
  Boost::log
  Boost::log_setup
  Boost::json
)

target_link_libraries(myexe PUBLIC
  Boost::unit_test_framework
  Boost::log
  Boost::log_setup
  Boost::locale
)

# copy the files
configure_file(test_one.py test_one.py)
configure_file(m.py m.py)       #server is here

# cmake .. && cmake --build .
add_custom_target(run ALL pytest -v -s COMMENT "Runing App 🐸 ")
# -s: show output

#[=[

   🦜 : This script starts the server and then use the client `myexe`. It
   calls Boost.test within pytest.

   🐢 : Wowoww


   cd ../../..
   cmake -S weak/include/net/test-client -B build-test-client
   cmake --build build-test-client
#]=]

