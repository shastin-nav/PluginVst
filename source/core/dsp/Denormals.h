#pragma once

// Scoped flush-to-zero / denormals-are-zero for the current thread.
//
// Decaying feedback loops (delay repeats, reverb tails) drift into subnormal
// numbers, which are up to ~100x slower on many CPUs. The engines enable FTZ
// themselves so they stay fast in every context (plug-in, tests, offline),
// and restore the previous mode afterwards.

#include <cstdint>

#if defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
    #include <xmmintrin.h>
    #define ECHOFORM_HAS_SSE_CSR 1
#elif (defined(__aarch64__) || defined(__arm64__)) && (defined(__GNUC__) || defined(__clang__))
    #define ECHOFORM_HAS_ARM64_FPCR 1
#endif

namespace echoform::dsp
{

class ScopedFlushDenormals
{
public:
    ScopedFlushDenormals() noexcept
    {
#if defined(ECHOFORM_HAS_SSE_CSR)
        previous = _mm_getcsr();
        _mm_setcsr (previous | 0x8040u); // FTZ (bit 15) | DAZ (bit 6)
#elif defined(ECHOFORM_HAS_ARM64_FPCR)
        std::uint64_t fpcr = 0;
        asm volatile ("mrs %0, fpcr" : "=r"(fpcr));
        previous = fpcr;
        asm volatile ("msr fpcr, %0" : : "r"(fpcr | (std::uint64_t { 1 } << 24))); // FZ
#endif
    }

    ~ScopedFlushDenormals()
    {
#if defined(ECHOFORM_HAS_SSE_CSR)
        _mm_setcsr (previous);
#elif defined(ECHOFORM_HAS_ARM64_FPCR)
        asm volatile ("msr fpcr, %0" : : "r"(previous));
#endif
    }

    ScopedFlushDenormals (const ScopedFlushDenormals&) = delete;
    ScopedFlushDenormals& operator= (const ScopedFlushDenormals&) = delete;

private:
#if defined(ECHOFORM_HAS_SSE_CSR)
    unsigned int previous = 0;
#elif defined(ECHOFORM_HAS_ARM64_FPCR)
    std::uint64_t previous = 0;
#endif
};

} // namespace echoform::dsp
