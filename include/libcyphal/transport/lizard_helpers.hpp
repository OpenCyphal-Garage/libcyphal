/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_LIZARD_HELPERS_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_LIZARD_HELPERS_HPP_INCLUDED

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>

namespace libcyphal
{
namespace transport
{

/// Internal implementation details of a lizard based transport.
/// Not supposed to be used directly by the users of the library.
///
namespace detail
{

class LizardHelpers final
{
public:
    LizardHelpers() = delete;

    /// Constructs a lizard C memory resource.
    ///
    template <typename MemoryResource, std::size_t Alignment = alignof(std::max_align_t)>
    CETL_NODISCARD static MemoryResource makeMemoryResource(cetl::pmr::memory_resource& memory)
    {
        /// No Sonar `cpp:S5356` is unavoidable - integration with Lizard C memory management.
        ///
        return {&memory, deallocateMemory<Alignment>, allocateMemory<Alignment>};  // NOSONAR cpp:S5356
    }

private:
    /// No Sonar `cpp:S5008` is unavoidable - integration with Lizard C memory management.
    ///
    template <std::size_t Alignment>
    static void* allocateMemory(void* const user_reference, const std::size_t amount)  // NOSONAR cpp:S5008
    {
        // No Sonar `cpp:S5357` "... isn't related to `void*`.
        // B/c we integrate here with lizard C memory management.
        auto* const memory = static_cast<cetl::pmr::memory_resource*>(user_reference);  // NOSONAR cpp:S5357
        CETL_DEBUG_ASSERT(nullptr != user_reference, "Expected PMR as non-null user reference.");

        return memory->allocate(amount, Alignment);
    }

    /// No Sonar `cpp:S5008` is unavoidable - integration with Lizard C memory management.
    ///
    template <std::size_t Alignment>
    static void deallocateMemory(void* const       user_reference,  // NOSONAR cpp:S5008
                                 const std::size_t amount,
                                 void* const       pointer)  // NOSONAR cpp:S5008
    {
        // No Sonar `cpp:S5357` "... isn't related to `void*`.
        // B/c we integrate here with lizard C memory management.
        auto* const memory = static_cast<cetl::pmr::memory_resource*>(user_reference);  // NOSONAR cpp:S5357
        CETL_DEBUG_ASSERT(nullptr != user_reference, "Expected PMR as non-null user reference.");

        memory->deallocate(pointer, amount, Alignment);
    }

};  // LizardHelpers

}  // namespace detail
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_LIZARD_HELPERS_HPP_INCLUDED
