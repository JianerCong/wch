set(RUN_SELECTED_ONLY OFF)
# set(RUN_SELECTED_ONLY ON)

#[=[

🦜 It feels like options() are read at config time, but set() is read at build
time? It appears like I have to rerun a "clean config+build" in order to take
the changed option() into effect

 #]=]
macro(set_test)
  # 🦜 : Below will create new lists ${l} and ${l-deps} if unpresent.
  list(APPEND l ${ARGV0})
  list(APPEND l-deps ${ARGV1})
endmacro()

#[=[
🦜 : Uncomment the following to enable tests.
#]=]
# set_test(test-core core-deps)
# set_test(test-core-20240130 core-deps)
# set_test(test-pure-thrd-party core-deps)
# set_test(test-executeEvmTx deps)
# set_test(test-mempool core-deps)
# set_test(test-sealer core-deps)
# set_test(test-storageManager deps)
# set_test(test-forPostExec core-deps)
# set_test(test-forPostExec-20240201 core-deps)
# set_test(test-execManager core-deps)

# set_test(test-forCnsss core-deps)
# set_test(test-pure-forCnsss core-deps)
# set_test(test-cnsssBlkChainAsstn core-deps)
# set_test(test-httpNetAsstn core-deps)
# set_test(test-httpNetAsstn2 core-deps)
# set_test(test-exeForCnsss core-deps)
# set_test(test-exeForCnsss2 core-deps)

## [deprecated:🦜 we have moved to WeakAsyncHttpServer, so enable test-pure-anet
## instead ] set_test(test-pure-net core-deps)

# set_test(test-pure-anet core-deps)
# set_test(test-rpc core-deps)
set_test(test-rpc-pb core-deps)
# set_test(test-init deps)
# set_test(test-inRamWrld deps)
# set_test(test-pure-greenClient core-deps)
# set_test(test-pure-rbft core-deps)
# set_test(test-pure-udp core-deps)
# set_test(test-udpNetAssnt core-deps)

# Add the above tests
foreach(t d IN ZIP_LISTS l l-deps)
  message(STATUS "Adding test: ${t}, deps=${d}")
  add_executable(${t} ${t}.cpp)

  target_link_libraries(${t} PUBLIC ${d})
  target_include_directories(${t} PUBLIC ${PROJECT_SOURCE_DIR}/include)

  # 🦜 : this won't work?? target_link_directories(test-all PUBLIC
  # ${PROJECT_SOURCE_DIR})

  # ?? still need to include ../core.hpp ??

  # [Solved]: 🐢: May be you should use: target_include_directories(test-all
  # PUBLIC ${PROJECT_SOURCE_DIR}/include)

  if (WIN32)
    set(exe ${PROJECT_BINARY_DIR}/test/Release/${t}.exe)
  else()
    set(exe ${PROJECT_BINARY_DIR}/test/${t})
  endif()

  if(RUN_SELECTED_ONLY)
    ## Option 1: test-only test marked with MY_TEST_THIS see test-hi.cpp
    ## --------------------------------------------------
    add_custom_target(run-${t} ALL ${exe}
      # --random
      --run_test=@aaa
      --log_level=all
      # --report_level=detailed
      COMMENT "Testing marked test 🐸")
  else()
    ## Option 2: test-all
    ## --------------------------------------------------
    add_custom_target(run-${t} ALL ${exe}
      --log_level=all COMMENT "Testing Tests 🐸")
  endif()
  add_dependencies(run-${t} ${t})
endforeach()


# message("project dir: ${PROJECT_SOURCE_DIR}")
# message("boost include dir: ${Boost_INCLUDE_DIRS}")
# message("boost library dir: ${Boost_LIBRARY_DIRS}")
# message("boost libraries: ${Boost_LIBRARIES}")
# message("boost log libraries: ${Boost_LOG_LIBRARY}")
# message("boost library dir release: ${Boost_LIBRARY_DIR_RELEASE}")

# --------------------------------------------------
# copy the example json files for testing
set(d0 ${CMAKE_CURRENT_SOURCE_DIR}/example-jsons)
set(d ${CMAKE_CURRENT_BINARY_DIR}/example-jsons)
file(GLOB l ${d0}/*.json)
message(STATUS "got l: ${l}")
file(MAKE_DIRECTORY ${d})
foreach(X IN LISTS l)
  file(RELATIVE_PATH x ${d0} ${X}) #get the file name
  message(STATUS "🐸 Copying ${X} to ${d}/${x}")
  configure_file(${X} ${d}/${x})
endforeach()

# add_executable(tps test_tps.cpp)
# target_link_libraries(tps PUBLIC core-deps)
