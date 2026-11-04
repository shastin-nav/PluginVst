#include "plugin/AllocationTracker.h"
#include "plugin/TestHelpers.h"

#include "params/ParameterSpecs.h"

#include <gtest/gtest.h>

#include <random>
#include <vector>

using namespace echoform;
using namespace echoform::test;

TEST (RealtimeSafety, AllocationTrackerDetectsHeapUse)
{
    // Positive control: proves the tracker really observes allocations, so a
    // zero count in the tests below is meaningful.
    // The pointer is published through a volatile so the compiler cannot elide
    // the new/delete pair (permitted since C++14).
    static int* volatile sink = nullptr;

    std::size_t allocations = 0;
    std::size_t deallocations = 0;
    {
        ScopedAllocationCounter counter;
        sink = new int (42);
        delete sink;
        {
            std::vector<float> v (100);
            sink = reinterpret_cast<int*> (v.data());
        }
        allocations = counter.allocations();
        deallocations = counter.deallocations();
    }
    sink = nullptr;

    EXPECT_EQ (allocations, 2u);
    EXPECT_EQ (deallocations, 2u);
}

TEST (RealtimeSafety, ProcessBlockNeverTouchesTheHeap)
{
    auto processor = makePreparedProcessor (48000.0, 512);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    std::mt19937 rng { 2024 };
    std::uniform_real_distribution<float> dist { 0.0f, 1.0f };

    for (int block = 0; block < 500; ++block)
    {
        // Host-side automation of every parameter between blocks (outside the
        // counted region: in a real host this happens on another thread).
        for (auto* param : processor->getParameters())
            if (block % 7 == 0)
                param->setValueNotifyingHost (dist (rng));

        const auto size = 1 + (block * 37) % 512;
        buffer.setSize (2, size, false, false, true);
        fillWithNoise (buffer, static_cast<unsigned> (block));

        std::size_t allocations = 0;
        std::size_t deallocations = 0;
        {
            ScopedAllocationCounter counter;
            processor->processBlock (buffer, midi);
            allocations = counter.allocations();
            deallocations = counter.deallocations();
        }
        ASSERT_EQ (allocations, 0u) << "allocation in processBlock, block " << block;
        ASSERT_EQ (deallocations, 0u) << "deallocation in processBlock, block " << block;
    }
}

TEST (RealtimeSafety, BypassedProcessingNeverTouchesTheHeap)
{
    auto processor = makePreparedProcessor (44100.0, 256);
    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;

    for (int block = 0; block < 50; ++block)
    {
        fillWithNoise (buffer, static_cast<unsigned> (block));
        std::size_t heapOperations = 0;
        {
            ScopedAllocationCounter counter;
            processor->processBlockBypassed (buffer, midi);
            heapOperations = counter.allocations() + counter.deallocations();
        }
        ASSERT_EQ (heapOperations, 0u);
    }
}
