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
    {
        canard_instance_ = canardInit(canardMemoryAllocate, canardMemoryFree);

        canard_instance_.user_reference = this;
        canard_instance_.node_id        = canard_node_id;
    }

private:
    // ICanTransport

    // ITransport

    CETL_NODISCARD cetl::optional<NodeId> getLocalNodeId() const noexcept override
    {
        return canard_instance_.node_id == CANARD_NODE_ID_UNSET ? cetl::nullopt
                                                                : cetl::make_optional(canard_instance_.node_id);
    }
    CETL_NODISCARD ProtocolParams getProtocolParams() const noexcept override
    {
        return ProtocolParams{};
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
    cetl::pmr::memory_resource& memory_;
    CanardInstance              canard_instance_;

    CETL_NODISCARD static inline TransportImpl& getSelfFrom(const CanardInstance* const ins)
    {
        CETL_DEBUG_ASSERT(ins != nullptr, "Expected canard instance.");
        CETL_DEBUG_ASSERT(ins->user_reference != nullptr, "Expected `this` transport as user reference.");

        return *static_cast<TransportImpl*>(ins->user_reference);
    }

    CETL_NODISCARD static void* canardMemoryAllocate(CanardInstance* ins, size_t amount)
    {
        auto& self = getSelfFrom(ins);
        return self.memory_.allocate(amount);
    }

    static void canardMemoryFree(CanardInstance* ins, void* pointer)
    {
        auto& self = getSelfFrom(ins);
        self.memory_.deallocate(pointer, 1);
    }

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
