# 🦜 : Some cmake-magic I just gonna paste it.
include(${CMAKE_CURRENT_LIST_DIR}/cmake-cable/bootstrap.cmake)
include(CableBuildType)
include(CableCompilerSettings)
include(CablePackage)
include(CableToolchains)

# cable_configure_toolchain(DEFAULT cxx17-pic)
# cable_set_build_type(DEFAULT Release CONFIGURATION_TYPES Release Debug)
cable_configure_compiler(NO_STACK_PROTECTION)
if(CABLE_COMPILER_GNULIKE)
    add_compile_options(
        -Wmissing-declarations
        -Wno-attributes  # Allow using unknown attributes.
        $<$<COMPILE_LANGUAGE:CXX>:-Wextra-semi>
    )
    cable_add_cxx_compiler_flag_if_supported(-Wduplicated-cond)
    cable_add_cxx_compiler_flag_if_supported(-Wduplicate-enum)
    cable_add_cxx_compiler_flag_if_supported(-Wfinal-dtor-non-final-class)
    cable_add_cxx_compiler_flag_if_supported(-Wlogical-op)
    cable_add_cxx_compiler_flag_if_supported(-Wnewline-eof)
    cable_add_cxx_compiler_flag_if_supported(-Wsuggest-destructor-override)
    cable_add_cxx_compiler_flag_if_supported(-Wunreachable-code-break)

    if(CABLE_COMPILER_CLANG)
        set(CMAKE_CXX_FLAGS_COVERAGE "-fprofile-instr-generate -fcoverage-mapping")
    elseif(CABLE_COMPILER_GNU)
        set(CMAKE_CXX_FLAGS_COVERAGE "--coverage")
    endif()
elseif(MSVC)
    add_compile_options(/wd4324)  # Disabled warning about alignment caused by alignas.
    add_compile_options(/wd5030)  # Allow using unknown attributes.
endif()

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL x86_64)
    # Setup options for x86_64 micro-architecture levels.
    # https://clang.llvm.org/docs/UsersManual.html#x86

    set(EVMWEAK_X86_64_ARCH_LEVEL_INIT 2)
    if(APPLE)
        # On macos with Apple Silicon CPU (arm64) the x86 is emulated and SSE4.2 is not available.
        set(EVMWEAK_X86_64_ARCH_LEVEL_INIT 1)
    endif()

    set(EVMWEAK_X86_64_ARCH_LEVEL ${EVMWEAK_X86_64_ARCH_LEVEL_INIT} CACHE STRING "The x86_64 micro-architecture level")
    if(EVMWEAK_X86_64_ARCH_LEVEL GREATER_EQUAL 1 AND EVMWEAK_X86_64_ARCH_LEVEL LESS_EQUAL 4)
        message(STATUS "x86_64 micro-architecture level: ${EVMWEAK_X86_64_ARCH_LEVEL}")
        if(EVMWEAK_X86_64_ARCH_LEVEL GREATER_EQUAL 2)
            add_compile_options(-march=x86-64-v${EVMWEAK_X86_64_ARCH_LEVEL})
        endif()
    else()
        message(FATAL_ERROR "Invalid EVMWEAK_X86_64_ARCH_LEVEL: ${EVMWEAK_X86_64_ARCH_LEVEL}")
    endif()
endif()
