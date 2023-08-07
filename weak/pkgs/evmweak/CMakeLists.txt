# evmweak: Fast Ethereum Virtual Machine implementation
# Copyright 2019 The evmweak Authors.
# SPDX-License-Identifier: Apache-2.0

# 🦜 : You should already have intx and ethash ready

include(config.cmake)
# ^^^^^ mainly disable the unknown attributes
add_subdirectory(evmc-10.1.0)
set(include_dir ${CMAKE_CURRENT_SOURCE_DIR}/include)

# find_package(intx CONFIG REQUIRED)
add_library(evmweak
    ${include_dir}/evmweak/evmweak.h
    advanced_analysis.cpp
    advanced_analysis.hpp
    advanced_execution.cpp
    advanced_execution.hpp
    advanced_instructions.cpp
    baseline.cpp
    baseline.hpp
    baseline_instruction_table.cpp
    baseline_instruction_table.hpp
    eof.cpp
    eof.hpp
    instructions.hpp
    instructions_calls.cpp
    instructions_opcodes.hpp
    instructions_storage.cpp
    instructions_traits.hpp
    instructions_xmacro.hpp
    opcodes_helpers.h
    tracing.cpp
    tracing.hpp
    vm.cpp
    vm.hpp
)
target_compile_features(evmweak PUBLIC cxx_std_20)
target_link_libraries(evmweak PUBLIC evmc::evmc intx::intx PRIVATE ethash::keccak)
target_include_directories(evmweak PUBLIC
    $<BUILD_INTERFACE:${include_dir}>$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(EVMWEAK_X86_64_ARCH_LEVEL GREATER_EQUAL 2)
    # Add CPU architecture runtime check. The EVMWEAK_X86_64_ARCH_LEVEL has a valid value.
    target_sources(evmweak PRIVATE cpu_check.cpp)
    set_source_files_properties(cpu_check.cpp PROPERTIES COMPILE_DEFINITIONS EVMWEAK_X86_64_ARCH_LEVEL=${EVMWEAK_X86_64_ARCH_LEVEL})
endif()

if(CABLE_COMPILER_GNULIKE)
    target_compile_options(
        evmweak PRIVATE
        -fno-exceptions
        $<$<CXX_COMPILER_ID:GNU>:-Wstack-usage=2600>
    )
    if(NOT SANITIZE MATCHES undefined)
        # RTTI can be disabled except for UBSan which checks vptr integrity.
        target_compile_options(evmweak PRIVATE -fno-rtti)
    endif()
endif()

set_target_properties(
    evmweak
    PROPERTIES
    VERSION ${PROJECT_VERSION}
    # SOVERSION ${PROJECT_SOVERSION}
)

if(NOT SANITIZE)
    # On Linux, check if all symbols in evmweak are resolved during linking.
    target_link_options(evmweak PRIVATE $<$<PLATFORM_ID:Linux>:LINKER:--no-undefined>)
endif()

set_source_files_properties(vm.cpp PROPERTIES COMPILE_DEFINITIONS PROJECT_VERSION="${PROJECT_VERSION}")

# include(LibraryTools)
# add_standalone_library(evmweak)
