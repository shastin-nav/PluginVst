#pragma once

// Detects heap activity on the current thread.
//
// The test executable replaces the global operator new/delete family
// (AllocationTracker.cpp). While a ScopedAllocationCounter is alive on a
// thread, every allocation and deallocation made by that thread is counted.
// This is how the tests prove processBlock() never touches the heap.

#include <cstddef>

namespace echoform::test
{

class ScopedAllocationCounter
{
public:
    ScopedAllocationCounter() noexcept;
    ~ScopedAllocationCounter();

    ScopedAllocationCounter (const ScopedAllocationCounter&) = delete;
    ScopedAllocationCounter& operator= (const ScopedAllocationCounter&) = delete;

    std::size_t allocations() const noexcept;
    std::size_t deallocations() const noexcept;

private:
    std::size_t allocationsAtStart;
    std::size_t deallocationsAtStart;
    bool previouslyEnabled;
};

} // namespace echoform::test
