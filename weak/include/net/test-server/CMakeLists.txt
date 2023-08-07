cmake_minimum_required(VERSION 3.21)
# set(CMAKE_CXX_COMPILER "g++")
project(hi VERSION 1.1)

# set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_BUILD_TYPE Release)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
IF (WIN32)
  # set stuff for windows
  set(Boost_DIR "C:\\Users\\congj\\repo\\boost_1_82_0\\stage\\lib\\cmake\\Boost-1.82.0")
  message("🐸 Setting Boost_DIR: ${Boost_DIR}")
ELSE()
  # set stuff for other systems
  set(Boost_USE_STATIC_LIBS ON)
  set(Boost_DIR "/home/me/.local/boost_1_82_0/stage/lib/cmake/Boost-1.82.0/")
ENDIF()

find_package(Boost 1.75...1.82
  CONFIG REQUIRED COMPONENTS json unit_test_framework log
)

add_executable(myexe test.cpp)
target_link_libraries(myexe PUBLIC Boost::log Boost::json)
# Although beast and axio are all header-only, we need to link to at least one
# Boost-target in order to bring in the include dirs

# copy the py file to build dir
configure_file(test_one.py test_one.py)

add_custom_target(run ALL pytest -v -s
  # test_one.py::test_serv_open_close
  # test_one.py::test_serv_log
  # test_one.py::test_serv_basic_request
  # test_one.py::
  # test_one.py::test_serv_dynamically_added_handler
  # test_one.py::test_serv_dynamically_remove_nonexisting_handler
  # test_one.py::test_serv_dynamically_remove_existing_handler
  # test_one.py::
  --last-failed                 #run the lastly failed test, or all if non-failed.
  COMMENT "Runing pytest 🐸")

# 🦜 : It looks like each individual test is passed but we have trouble when
# running them in serial.

#[=[

  Example

  pyenv
  cd ../../../../
  rm build-test-net -rf && cmake -S weak/include/net/test-server -B build-test-net
  cmake --build build-test-net

🦜: Sometimes it fails to connect, try it again maybe.

platform linux -- Python 3.10.6, pytest-7.1.2, pluggy-1.0.0 -- /home/me/work/lcode/myenv/bin/python3
cachedir: .pytest_cache
rootdir: /home/me/repo/notes/build-test-net
collected 7 items

test_one.py::test_serv_open_close PASSED                                                                                                               [ 14%]
test_one.py::test_serv_log PASSED                                                                                                                      [ 28%]
test_one.py::test_serv_basic_404 PASSED                                                                                                                [ 42%]
test_one.py::test_serv_basic_request PASSED                                                                                                            [ 57%]
test_one.py::test_serv_dynamically_added_handler PASSED                                                                                                [ 71%]
test_one.py::test_serv_dynamically_remove_nonexisting_handler PASSED                                                                                   [ 85%]
test_one.py::test_serv_dynamically_remove_existing_handler PASSED                                                                                      [100%]

===================================================================== 7 passed in 14.11s =====================================================================
[100%] Built target run


 #]=]
