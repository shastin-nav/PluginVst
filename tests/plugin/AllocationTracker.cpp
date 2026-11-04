#include "plugin/AllocationTracker.h"

#include <cstdlib>
#include <new>

#if defined(_WIN32)
    #include <malloc.h>
#endif

namespace
{
thread_local bool trackingEnabled = false;
thread_local std::size_t allocationCount = 0;
thread_local std::size_t deallocationCount = 0;

void noteAllocation() noexcept
{
    if (trackingEnabled)
        ++allocationCount;
}

void noteDeallocation() noexcept
{
    if (trackingEnabled)
        ++deallocationCount;
}

void* allocate (std::size_t size)
{
    noteAllocation();
    if (auto* p = std::malloc (size == 0 ? 1 : size))
        return p;
    throw std::bad_alloc();
}

void* allocateAligned (std::size_t size, std::size_t alignment)
{
    noteAllocation();
    if (size == 0)
        size = alignment;

#if defined(_WIN32)
    if (auto* p = _aligned_malloc (size, alignment))
        return p;
#else
    void* p = nullptr;
    if (posix_memalign (&p, alignment < sizeof (void*) ? sizeof (void*) : alignment, size) == 0)
        return p;
#endif
    throw std::bad_alloc();
}

void release (void* p) noexcept
{
    if (p == nullptr)
        return;
    noteDeallocation();
    std::free (p);
}

void releaseAligned (void* p) noexcept
{
    if (p == nullptr)
        return;
    noteDeallocation();
#if defined(_WIN32)
    _aligned_free (p);
#else
    std::free (p);
#endif
}
} // namespace

namespace echoform::test
{
ScopedAllocationCounter::ScopedAllocationCounter() noexcept
    : allocationsAtStart (allocationCount),
      deallocationsAtStart (deallocationCount),
      previouslyEnabled (trackingEnabled)
{
    trackingEnabled = true;
}

ScopedAllocationCounter::~ScopedAllocationCounter() { trackingEnabled = previouslyEnabled; }

std::size_t ScopedAllocationCounter::allocations() const noexcept { return allocationCount - allocationsAtStart; }

std::size_t ScopedAllocationCounter::deallocations() const noexcept { return deallocationCount - deallocationsAtStart; }
} // namespace echoform::test

// ---- Replacement global allocation functions --------------------------------------------------

void* operator new (std::size_t size) { return allocate (size); }
void* operator new[] (std::size_t size) { return allocate (size); }

void* operator new (std::size_t size, const std::nothrow_t&) noexcept
{
    try { return allocate (size); } catch (...) { return nullptr; }
}

void* operator new[] (std::size_t size, const std::nothrow_t&) noexcept
{
    try { return allocate (size); } catch (...) { return nullptr; }
}

void* operator new (std::size_t size, std::align_val_t al) { return allocateAligned (size, static_cast<std::size_t> (al)); }
void* operator new[] (std::size_t size, std::align_val_t al) { return allocateAligned (size, static_cast<std::size_t> (al)); }

void* operator new (std::size_t size, std::align_val_t al, const std::nothrow_t&) noexcept
{
    try { return allocateAligned (size, static_cast<std::size_t> (al)); } catch (...) { return nullptr; }
}

void* operator new[] (std::size_t size, std::align_val_t al, const std::nothrow_t&) noexcept
{
    try { return allocateAligned (size, static_cast<std::size_t> (al)); } catch (...) { return nullptr; }
}

void operator delete (void* p) noexcept { release (p); }
void operator delete[] (void* p) noexcept { release (p); }
void operator delete (void* p, std::size_t) noexcept { release (p); }
void operator delete[] (void* p, std::size_t) noexcept { release (p); }
void operator delete (void* p, const std::nothrow_t&) noexcept { release (p); }
void operator delete[] (void* p, const std::nothrow_t&) noexcept { release (p); }

void operator delete (void* p, std::align_val_t) noexcept { releaseAligned (p); }
void operator delete[] (void* p, std::align_val_t) noexcept { releaseAligned (p); }
void operator delete (void* p, std::size_t, std::align_val_t) noexcept { releaseAligned (p); }
void operator delete[] (void* p, std::size_t, std::align_val_t) noexcept { releaseAligned (p); }
void operator delete (void* p, std::align_val_t, const std::nothrow_t&) noexcept { releaseAligned (p); }
void operator delete[] (void* p, std::align_val_t, const std::nothrow_t&) noexcept { releaseAligned (p); }
