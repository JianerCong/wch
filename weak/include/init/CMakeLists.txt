cmake_minimum_required(VERSION 3.10)

# set the project name and version
project(Hi VERSION 1.0)

if (WIN32)
  # set stuff for windows
  set(Boost_DIR "C:\\Users\\congj\\repo\\boost_1_82_0\\stage\\lib\\cmake\\Boost-1.82.0")
  message("🐸 Setting Boost_DIR: ${Boost_DIR}")
else()
  set(Boost_USE_STATIC_LIBS ON)
  set(Boost_DIR "/home/me/.local/boost_1_82_0/stage/lib/cmake/Boost-1.82.0/")
endif()

find_package(Boost CONFIG REQUIRED
  unit_test_framework program_options)
# add the executable

add_executable(myexe testOptions.cpp)
target_link_libraries(myexe PUBLIC
  Boost::unit_test_framework
  Boost::program_options)

# copy the test
configure_file(test_1.py test_1.py)

add_custom_target(run ALL pytest COMMENT "Runing pytest 🐸")
# The only file exposed to the outside world is options.hpp and the rest of the
# files in this folder is for testing.


#[=[

cd ../../../
pyenv

--------------------------------------------------
cmake -S weak/include/init/ -B build-test-options
cmake --build build-test-options
Or use: ------------------------------------------
. ./s-options.sh
#]=]
