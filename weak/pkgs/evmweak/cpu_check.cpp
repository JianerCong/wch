
#define STRINGIFY_HELPER(X) #X
#define STRINGIFY(X) STRINGIFY_HELPER(X)

#if defined(__GNUC__) && __GNUC__ >= 12
#define CPU_FEATURE "x86-64-v" STRINGIFY(EVMWEAK_X86_64_ARCH_LEVEL)
#else
// Clang 16 and GCC 11 does not support architecture levels in __builtin_cpu_supports().
// Use approximations.
#if EVMWEAK_X86_64_ARCH_LEVEL == 2
#define CPU_FEATURE "sse4.2"
#endif
#endif

#ifndef CPU_FEATURE
#error "EVMWEAK_X86_64_ARCH_LEVEL: Unsupported x86-64 architecture level"
#endif

#include <cstdio>
#include <cstdlib>

static bool cpu_check = []() noexcept {
    if (!__builtin_cpu_supports(CPU_FEATURE))
    {
        (void)std::fputs("CPU does not support " CPU_FEATURE "\n", stderr);
        std::abort();
    }
    return false;
}();
