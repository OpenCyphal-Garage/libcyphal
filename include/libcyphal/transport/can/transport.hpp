/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_CAN_TRANSPORT_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_TRANSPORT_HPP_INCLUDED

#include "media.hpp"
#include "libcyphal/transport/transport.hpp"
#include "libcyphal/transport/multiplexer.hpp"

#include <canard.h>

namespace libcyphal
{
namespace transport
{
namespace can
{

class ICanTransport : public ITransport
{};

namespace detail
{

class TransportImpl final : public ICanTransport
{
public:
    TransportImpl(cetl::pmr::memory_resource& memory, const CanardNodeID canard_node_id)
        : memory_{memory}
        , canard_instance_{canardInit(canardMemoryAllocate, canardMemoryFree)}
    {
        canard_instance_.user_reference = this;
        canard_instance_.node_id        = canard_node_id;
    }

    // ICanTransport

    // ITransport

    CETL_NODISCARD cetl::optional<NodeId> getLocalNodeId() const noexcept override
    {
        return canard_instance_.node_id > CANARD_NODE_ID_MAX ? cetl::nullopt
                                                             : cetl::make_optional(canard_instance_.node_id);
    }
    CETL_NODISCARD ProtocolParams getProtocolParams() const noexcept override
    {
        // TODO: Fill MTU
        return ProtocolParams{1 << CANARD_TRANSFER_ID_BIT_LENGTH, 0, CANARD_NODE_ID_MAX + 1};
    }

    CETL_NODISCARD Expected<UniquePtr<IMessageRxSession>, AnyError> makeMessageRxSession(
        const MessageRxParams&) override
    {
        return NotImplementedError{};
    }
    CETL_NODISCARD Expected<UniquePtr<IMessageTxSession>, AnyError> makeMessageTxSession(
        const MessageTxParams&) override
    {
        return NotImplementedError{};
    }
    CETL_NODISCARD Expected<UniquePtr<IRequestRxSession>, AnyError> makeRequestRxSession(
        const RequestRxParams&) override
    {
        return NotImplementedError{};
    }
    CETL_NODISCARD Expected<UniquePtr<IRequestTxSession>, AnyError> makeRequestTxSession(
        const RequestTxParams&) override
    {
        return NotImplementedError{};
    }
    CETL_NODISCARD Expected<UniquePtr<IResponseRxSession>, AnyError> makeResponseRxSession(
        const ResponseRxParams&) override
    {
        return NotImplementedError{};
    }
    CETL_NODISCARD Expected<UniquePtr<IResponseTxSession>, AnyError> makeResponseTxSession(
        const ResponseTxParams&) override
    {
        return NotImplementedError{};
    }

    // IRunnable

    void run(const TimePoint) override {}

private:
    // Until "canardMemFree must provide size" issue #216 is resolved,
    // we need to store the size of the memory allocated.
    // TODO: Remove this workaround when the issue is resolved.
    // see https://github.com/OpenCyphal/libcanard/issues/216
    //
    struct CanardMemory
    {
        struct Layout
        {
            std::size_t size;
            alignas(std::max_align_t) cetl::byte data[sizeof(std::max_align_t)];
        };

        static constexpr std::size_t DataOffset = sizeof(Layout) - sizeof(Layout::data);
    };

    CETL_NODISCARD static inline TransportImpl& getSelfFrom(const CanardInstance* const ins)
    {
        CETL_DEBUG_ASSERT(ins != nullptr, "Expected canard instance.");
        CETL_DEBUG_ASSERT(ins->user_reference != nullptr, "Expected `this` transport as user reference.");

        return *static_cast<TransportImpl*>(ins->user_reference);
    }

    CETL_NODISCARD static void* canardMemoryAllocate(CanardInstance* ins, size_t amount)
    {
        auto& self = getSelfFrom(ins);

        const auto memory_size   = CanardMemory::DataOffset + amount;
        const auto canard_memory = static_cast<CanardMemory::Layout*>(self.memory_.allocate(memory_size));
        if (canard_memory == nullptr)
        {
            return nullptr;
        }
        canard_memory->size = memory_size;

        return &canard_memory->data;
    }

    static void canardMemoryFree(CanardInstance* ins, void* pointer)
    {
        if (pointer == nullptr)
        {
            return;
        }

        const auto uint_ptr = reinterpret_cast<std::uintptr_t>(pointer);
        CETL_DEBUG_ASSERT(uint_ptr > CanardMemory::DataOffset, "Invalid too small pointer.");
        const auto canard_memory = reinterpret_cast<CanardMemory::Layout*>(uint_ptr - CanardMemory::DataOffset);

        auto& self = getSelfFrom(ins);
        self.memory_.deallocate(canard_memory, canard_memory->size);
    }

    cetl::pmr::memory_resource& memory_;
    CanardInstance              canard_instance_;

};  // TransportImpl

}  // namespace detail

CETL_NODISCARD inline Expected<UniquePtr<ICanTransport>, FactoryError> makeTransport(
    cetl::pmr::memory_resource&  memory,
    IMultiplexer&                mux,
    const std::array<IMedia*, 3> media,  // TODO: replace with `cetl::span<IMedia*>`
    const cetl::optional<NodeId> local_node_id)
{
    // TODO: Use these!
    (void) mux;
    (void) media;

    if (local_node_id.has_value() && local_node_id.value() > CANARD_NODE_ID_MAX)
    {
        return ArgumentError{};
    }
    const auto canard_node_id = static_cast<CanardNodeID>(local_node_id.value_or(CANARD_NODE_ID_UNSET));

    cetl::pmr::polymorphic_allocator<detail::TransportImpl> allocator{&memory};

    auto transport = cetl::pmr::Factory::make_unique(allocator, memory, canard_node_id);

    return UniquePtr<ICanTransport>{transport.release(), UniquePtr<ICanTransport>::deleter_type{allocator, 1}};
}

}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_TRANSPORT_HPP_INCLUDED
