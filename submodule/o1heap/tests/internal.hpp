// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
// documentation files (the "Software"), to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
// and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions
// of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
// WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
// OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// Copyright (c) 2020 Pavel Kirienko
// Authors: Pavel Kirienko <pavel.kirienko@zubax.com>

#ifndef O1HEAP_TESTS_INTERNAL_HPP_INCLUDED
#define O1HEAP_TESTS_INTERNAL_HPP_INCLUDED

#include "catch.hpp"
#include "o1heap.h"
#include <array>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
#include <sstream>
#include <vector>

/// Definitions that are not exposed by the library but that are needed for testing.
/// Please keep them in sync with the library by manually updating as necessary.
namespace internal
{
extern "C" {
auto log2Floor(const std::size_t x) -> std::uint8_t;
auto log2Ceil(const std::size_t x) -> std::uint8_t;
auto pow2(const std::uint8_t power) -> std::size_t;
auto roundUpToPowerOf2(const std::size_t x) -> std::size_t;
}

struct Fragment;

struct FragmentHeader final
{
    Fragment*   next = nullptr;
    Fragment*   prev = nullptr;
    std::size_t size = 0U;
    bool        used = false;
};

struct Fragment final
{
    FragmentHeader header;

    Fragment* next_free = nullptr;
    Fragment* prev_free = nullptr;

    static constexpr auto SizeMin = O1HEAP_ALIGNMENT * 2U;
    static constexpr auto SizeMax = (std::numeric_limits<std::size_t>::max() >> 1U) + 1U;
    static_assert((SizeMin & (SizeMin - 1U)) == 0U);
    static_assert((SizeMax & (SizeMax - 1U)) == 0U);

    [[nodiscard]] static auto constructFromAllocatedMemory(const void* const memory) -> const Fragment&
    {
        if ((memory == nullptr) || (reinterpret_cast<std::size_t>(memory) <= O1HEAP_ALIGNMENT) ||
            (reinterpret_cast<std::size_t>(memory) % O1HEAP_ALIGNMENT) != 0U)
        {
            throw std::invalid_argument("Invalid pointer");
        }
        return *reinterpret_cast<const Fragment*>(
            reinterpret_cast<const void*>(reinterpret_cast<const std::byte*>(memory) - O1HEAP_ALIGNMENT));
    }

    [[nodiscard]] auto getBinIndex() const -> std::uint8_t
    {
        const bool aligned  = (header.size % SizeMin) == 0U;
        const bool nonempty = header.size >= SizeMin;
        if (aligned && nonempty)
        {
            return static_cast<std::uint8_t>(std::floor(std::log2(header.size / SizeMin)));
        }
        throw std::logic_error("Invalid fragment size");
    }

    void validate() const
    {
        const auto address = reinterpret_cast<std::size_t>(this);
        REQUIRE((address % sizeof(void*)) == 0U);

        // Size correctness.
        REQUIRE(header.size >= SizeMin);
        REQUIRE(header.size <= SizeMax);
        REQUIRE((header.size % SizeMin) == 0U);

        // Heap fragment interlinking. Free blocks cannot neighbor each other because they are supposed to be merged.
        if (header.next != nullptr)
        {
            REQUIRE((header.used || header.next->header.used));
            const auto adr = reinterpret_cast<std::size_t>(header.next);
            REQUIRE((adr % sizeof(void*)) == 0U);
            REQUIRE(header.next->header.prev == this);
            REQUIRE(adr > address);
            REQUIRE(((adr - address) % SizeMin) == 0U);
        }
        if (header.prev != nullptr)
        {
            REQUIRE((header.used || header.prev->header.used));
            const auto adr = reinterpret_cast<std::size_t>(header.prev);
            REQUIRE((adr % sizeof(void*)) == 0U);
            REQUIRE(header.prev->header.next == this);
            REQUIRE(address > adr);
            REQUIRE(((address - adr) % SizeMin) == 0U);
        }

        // Segregated free list interlinking.
        if (!header.used)
        {
            if (next_free != nullptr)
            {
                REQUIRE(next_free->prev_free == this);
                REQUIRE(!next_free->header.used);
            }
            if (prev_free != nullptr)
            {
                REQUIRE(prev_free->next_free == this);
                REQUIRE(!prev_free->header.used);
            }
        }
    }

    Fragment()                                    = delete;
    Fragment(const Fragment&)                     = delete;
    Fragment(const Fragment&&)                    = delete;
    ~Fragment()                                   = delete;
    auto operator=(const Fragment&) -> Fragment&  = delete;
    auto operator=(const Fragment&&) -> Fragment& = delete;
};

/// Please maintain the fields in exact sync with the private definition in o1heap.c!
struct O1HeapInstance final
{
    std::array<Fragment*, sizeof(std::size_t) * 8U> bins{};

    std::size_t nonempty_bin_mask = 0;

    /// The same data is available via getDiagnostics(). The duplication is intentional.
    O1HeapDiagnostics diagnostics{};

    [[nodiscard]] auto allocate(const size_t amount)
    {
        validate();  // Can't use RAII because it may throw -- can't throw from destructor.
        const auto out = o1heapAllocate(reinterpret_cast<::O1HeapInstance*>(this), amount);
        if (out != nullptr)
        {
            Fragment::constructFromAllocatedMemory(out).validate();
        }
        validate();
        return out;
    }

    auto free(void* const pointer)
    {
        validate();
        o1heapFree(reinterpret_cast<::O1HeapInstance*>(this), pointer);
        validate();
    }

    [[nodiscard]] auto doInvariantsHold() const
    {
        return o1heapDoInvariantsHold(reinterpret_cast<const ::O1HeapInstance*>(this));
    }

    [[nodiscard]] auto getDiagnostics() const
    {
        validate();
        const auto out = o1heapGetDiagnostics(reinterpret_cast<const ::O1HeapInstance*>(this));
        validate();
        REQUIRE(std::memcmp(&diagnostics, &out, sizeof(diagnostics)) == 0);
        return out;
    }

    [[nodiscard]] auto getFirstFragment() const
    {
        const std::uint8_t* ptr = reinterpret_cast<const std::uint8_t*>(this) + sizeof(*this);
        while ((reinterpret_cast<std::size_t>(ptr) % O1HEAP_ALIGNMENT) != 0)
        {
            ptr++;
        }
        const auto frag = reinterpret_cast<const Fragment*>(reinterpret_cast<const void*>(ptr));
        // Apply heuristics to make sure the fragment is found correctly.
        REQUIRE(frag->header.size >= Fragment::SizeMin);
        REQUIRE(frag->header.size <= Fragment::SizeMax);
        REQUIRE(frag->header.size <= diagnostics.capacity);
        REQUIRE((frag->header.size % Fragment::SizeMin) == 0U);
        REQUIRE(((frag->header.next == nullptr) || (frag->header.next->header.prev == frag)));
        REQUIRE(frag->header.prev == nullptr);  // The first fragment has no prev!
        return frag;
    }

    void validate() const
    {
        validateCore();
        validateFragmentChain();
        validateSegregatedFreeLists();
    }

    /// A list of fragment descriptors to match the heap state against.
    /// The boolean is true if the fragment shall be used (allocated); the size is its size in bytes, overhead included.
    /// If the size is zero, it will be ignored (i.e., any value will match).
    void matchFragments(const std::vector<std::pair<bool, std::size_t>>& reference) const
    {
        validate();
        INFO(visualize());
        auto frag = getFirstFragment();
        for (auto item : reference)
        {
            const auto [used, size] = item;
            CAPTURE(used, size, frag);
            REQUIRE(frag != nullptr);
            REQUIRE(frag->header.used == used);
            CAPTURE(frag->header.size);
            REQUIRE((((size == 0U) || (frag->header.size == size))));
            REQUIRE(((frag->header.next == nullptr) || (frag->header.next->header.prev == frag)));
            frag = frag->header.next;
        }
        REQUIRE(frag == nullptr);
    }

    [[nodiscard]] auto visualize() const -> std::string
    {
        std::stringstream buffer;
        buffer << "Heap diagnostics: "
               << "capacity=" << diagnostics.capacity << " B, "
               << "allocated=" << diagnostics.allocated << " B, "
               << "peak_allocated=" << diagnostics.peak_allocated << " B, "
               << "peak_request_size=" << diagnostics.peak_request_size << " B, "
               << "oom_count=" << diagnostics.oom_count << ".\n"
               << "Size of used blocks is printed as-is, size of free blocks is printed in [brackets]. "
               << "All sizes are divided by the min fragment size (" << Fragment::SizeMin << " bytes).\n";
        auto frag = getFirstFragment();
        do
        {
            const auto size_blocks = frag->header.size / Fragment::SizeMin;
            if (frag->header.used)
            {
                buffer << size_blocks << " ";
            }
            else
            {
                buffer << "[" << size_blocks << "] ";
            }
            frag = frag->header.next;
        } while (frag != nullptr);
        buffer << "\n";
        return buffer.str();
    }

    O1HeapInstance()                                          = delete;
    O1HeapInstance(const O1HeapInstance&)                     = delete;
    O1HeapInstance(const O1HeapInstance&&)                    = delete;
    ~O1HeapInstance()                                         = delete;
    auto operator=(const O1HeapInstance&) -> O1HeapInstance&  = delete;
    auto operator=(const O1HeapInstance&&) -> O1HeapInstance& = delete;

private:
    void validateCore() const
    {
        REQUIRE(diagnostics.capacity >= Fragment::SizeMin);
        REQUIRE(diagnostics.capacity <= Fragment::SizeMax);
        REQUIRE((diagnostics.capacity % Fragment::SizeMin) == 0U);

        REQUIRE(diagnostics.allocated <= diagnostics.capacity);
        REQUIRE((diagnostics.allocated % Fragment::SizeMin) == 0U);

        REQUIRE(diagnostics.peak_allocated <= diagnostics.capacity);
        REQUIRE(diagnostics.peak_allocated >= diagnostics.allocated);
        REQUIRE((diagnostics.peak_allocated % Fragment::SizeMin) == 0U);

        REQUIRE(((diagnostics.peak_request_size <= diagnostics.capacity) || (diagnostics.oom_count > 0U)));
        REQUIRE((((diagnostics.peak_request_size + O1HEAP_ALIGNMENT) <= diagnostics.peak_allocated) ||
                 (diagnostics.peak_request_size == 0U) || (diagnostics.oom_count > 0U)));
    }

    void validateFragmentChain() const
    {
        std::size_t pending_bins = 0U;
        for (std::size_t i = 0U; i < std::size(bins); i++)
        {
            if (bins.at(i) != nullptr)
            {
                pending_bins |= static_cast<std::size_t>(1) << i;
            }
        }
        // Ensure the bin lookup mask is in sync with the bins.
        REQUIRE(pending_bins == nonempty_bin_mask);

        std::size_t total_size      = 0U;
        std::size_t total_allocated = 0U;

        auto frag = getFirstFragment();
        do
        {
            frag->validate();
            REQUIRE(frag->header.size <= diagnostics.capacity);

            // Update and check the totals early.
            total_size += frag->header.size;
            REQUIRE(total_size <= Fragment::SizeMax);
            REQUIRE(total_size <= diagnostics.capacity);
            REQUIRE((total_size % Fragment::SizeMin) == 0U);
            if (frag->header.used)
            {
                total_allocated += frag->header.size;
                REQUIRE(total_allocated <= total_size);
                REQUIRE((total_allocated % Fragment::SizeMin) == 0U);
                // Ensure no bin links to a used fragment.
                REQUIRE(bins.at(frag->getBinIndex()) != frag);
            }
            else
            {
                const std::size_t mask = static_cast<std::size_t>(1) << frag->getBinIndex();
                REQUIRE((nonempty_bin_mask & mask) != 0U);
                if (bins.at(frag->getBinIndex()) == frag)
                {
                    REQUIRE((pending_bins & mask) != 0U);
                    pending_bins &= ~mask;
                }
            }

            frag = frag->header.next;
        } while (frag != nullptr);

        // Ensure there were no hanging bin pointers.
        REQUIRE(pending_bins == 0);

        // Validate the totals.
        REQUIRE(total_size == diagnostics.capacity);
        REQUIRE(total_allocated == diagnostics.allocated);
    }

    void validateSegregatedFreeLists() const
    {
        std::size_t total_free = 0U;
        for (std::size_t i = 0U; i < std::size(bins); i++)
        {
            const std::size_t mask = static_cast<std::size_t>(1) << i;
            const std::size_t min  = Fragment::SizeMin << i;
            const std::size_t max  = (Fragment::SizeMin << i) * 2U - 1U;

            auto frag = bins.at(i);
            if (frag != nullptr)
            {
                REQUIRE((nonempty_bin_mask & mask) != 0U);
                REQUIRE(!frag->header.used);
                REQUIRE(frag->prev_free == nullptr);  // The first fragment in the segregated list has no prev.
                do
                {
                    REQUIRE(frag->header.size >= min);
                    REQUIRE(frag->header.size <= max);

                    total_free += frag->header.size;

                    if (frag->next_free != nullptr)
                    {
                        REQUIRE(frag->next_free->prev_free == frag);
                        REQUIRE(!frag->next_free->header.used);
                    }
                    if (frag->prev_free != nullptr)
                    {
                        REQUIRE(frag->prev_free->next_free == frag);
                        REQUIRE(!frag->prev_free->header.used);
                    }

                    frag = frag->next_free;
                } while (frag != nullptr);
            }
            else
            {
                REQUIRE((nonempty_bin_mask & mask) == 0U);
            }
        }
        REQUIRE((diagnostics.capacity - diagnostics.allocated) == total_free);
    }
};

static_assert(O1HEAP_VERSION_MAJOR == 2);

}  // namespace internal

#endif  // O1HEAP_TESTS_INTERNAL_HPP_INCLUDED
