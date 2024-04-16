/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_CAN_TRANSPORT_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_TRANSPORT_HPP_INCLUDED

#include "media.hpp"
#include "delegate.hpp"
#include "msg_rx_session.hpp"
#include "libcyphal/transport/transport.hpp"
#include "libcyphal/transport/multiplexer.hpp"

#include <canard.h>

#include <algorithm>

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
class TransportImpl final : public ICanTransport, private TransportDelegate
{
    // In use to disable public construction.
    // See https://seanmiddleditch.github.io/enabling-make-unique-with-private-constructors/
    struct Tag
    {
        explicit Tag()  = default;
        using Interface = ICanTransport;
        using Concrete  = TransportImpl;
    };

public:
    CETL_NODISCARD static Expected<UniquePtr<ICanTransport>, FactoryError> make(
        cetl::pmr::memory_resource&                    memory,
        IMultiplexer&                                  multiplexer,
        const std::array<IMedia*, MaxMediaInterfaces>& media,
        const cetl::optional<NodeId>                   local_node_id)
    {
        // Verify input arguments:
        // - At least one media interface must be provided.
        // - If a local node ID is provided, it must be within the valid range.
        //
        const auto media_count = static_cast<std::size_t>(
            std::count_if(media.cbegin(), media.cend(), [](auto media) { return media != nullptr; }));
        if (media_count == 0)
        {
            return ArgumentError{};
        }
        if (local_node_id.has_value() && local_node_id.value() > CANARD_NODE_ID_MAX)
        {
            return ArgumentError{};
        }

        libcyphal::detail::VarArray<IMedia*> media_array{MaxMediaInterfaces, &memory};
        media_array.reserve(media_count);
        std::copy_if(media.cbegin(), media.cend(), std::back_inserter(media_array), [](auto media) {
            return media != nullptr;
        });
        CETL_DEBUG_ASSERT(!media_array.empty() && (media_array.size() == media_count), "");

        const auto canard_node_id = static_cast<CanardNodeID>(local_node_id.value_or(CANARD_NODE_ID_UNSET));

        return libcyphal::detail::makeUniquePtr<Tag>(memory,
                                                     Tag{},
                                                     memory,
                                                     multiplexer,
                                                     std::move(media_array),
                                                     canard_node_id);
    }

    TransportImpl(Tag,
                  cetl::pmr::memory_resource&            memory,
                  IMultiplexer&                          multiplexer,
                  libcyphal::detail::VarArray<IMedia*>&& media_array,
                  const CanardNodeID                     canard_node_id)
        : TransportDelegate{memory, canardInit(canardMemoryAllocate, canardMemoryFree)}
        , media_array_{std::move(media_array)}
    {
        // TODO: Use it!
        (void) multiplexer;

        canard_instance_.user_reference = this;
        canard_instance_.node_id        = canard_node_id;
    }

private:
    // MARK: ICanTransport

    // MARK: ITransport

    CETL_NODISCARD cetl::optional<NodeId> getLocalNodeId() const noexcept override
    {
        return canard_instance_.node_id > CANARD_NODE_ID_MAX ? cetl::nullopt
                                                             : cetl::make_optional(canard_instance_.node_id);
    }
    CETL_NODISCARD ProtocolParams getProtocolParams() const noexcept override
    {
        const auto min_mtu = reduceMedia(std::numeric_limits<std::size_t>::max(),
                                         [](auto mtu, IMedia& media) { return std::min(mtu, media.getMtu()); });

        return ProtocolParams{1 << CANARD_TRANSFER_ID_BIT_LENGTH, min_mtu, CANARD_NODE_ID_MAX + 1};
    }

    CETL_NODISCARD Expected<UniquePtr<IMessageRxSession>, AnyError> makeMessageRxSession(
        const MessageRxParams& params) override
    {
        auto any_error = ensureNewSessionFor(CanardTransferKindMessage, params.subject_id, CANARD_SUBJECT_ID_MAX);
        if (any_error.has_value())
        {
            return any_error.value();
        }

        return MessageRxSession::make(asDelegate(), params);
    }

    CETL_NODISCARD Expected<UniquePtr<IMessageTxSession>, AnyError> makeMessageTxSession(
        const MessageTxParams&) override
    {
        return NotImplementedError{};
    }

    CETL_NODISCARD Expected<UniquePtr<IRequestRxSession>, AnyError> makeRequestRxSession(
        const RequestRxParams& params) override
    {
        auto any_error = ensureNewSessionFor(CanardTransferKindRequest, params.service_id, CANARD_SERVICE_ID_MAX);
        if (any_error.has_value())
        {
            return any_error.value();
        }

        return NotImplementedError{};
    }

    CETL_NODISCARD Expected<UniquePtr<IRequestTxSession>, AnyError> makeRequestTxSession(
        const RequestTxParams&) override
    {
        return NotImplementedError{};
    }

    CETL_NODISCARD Expected<UniquePtr<IResponseRxSession>, AnyError> makeResponseRxSession(
        const ResponseRxParams& params) override
    {
        auto any_error = ensureNewSessionFor(CanardTransferKindResponse, params.service_id, CANARD_SERVICE_ID_MAX);
        if (any_error.has_value())
        {
            return any_error.value();
        }

        return NotImplementedError{};
    }

    CETL_NODISCARD Expected<UniquePtr<IResponseTxSession>, AnyError> makeResponseTxSession(
        const ResponseTxParams&) override
    {
        return NotImplementedError{};
    }

    // MARK: IRunnable

    void run(const TimePoint) override {}

    // MARK: TransportDelegate

    CETL_NODISCARD inline TransportDelegate& asDelegate()
    {
        return *this;
    }

    // MARK: Privates:

    // Until "canardMemFree must provide size" issue #216 is resolved,
    // we need to store the size of the memory allocated.
    // TODO: Remove this workaround when the issue is resolved.
    // see https://github.com/OpenCyphal/libcanard/issues/216
    //
    struct CanardMemory
    {
        alignas(std::max_align_t) std::size_t size;
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

        const auto memory_size   = sizeof(CanardMemory) + amount;
        auto       canard_memory = static_cast<CanardMemory*>(self.memory_.allocate(memory_size));
        if (canard_memory == nullptr)
        {
            return nullptr;
        }

        // Return the memory after the `CanardMemory` struct (containing its size).
        // The size is used in `canardMemoryFree` to deallocate the memory.
        //
        canard_memory->size = memory_size;
        return ++canard_memory;
    }

    static void canardMemoryFree(CanardInstance* ins, void* pointer)
    {
        if (pointer == nullptr)
        {
            return;
        }

        auto canard_memory = static_cast<CanardMemory*>(pointer);
        --canard_memory;

        auto& self = getSelfFrom(ins);
        self.memory_.deallocate(canard_memory, canard_memory->size);
    }

    template <typename Action>
    void forEachMedia(Action action)
    {
        for (const auto media : media_array_)
        {
            CETL_DEBUG_ASSERT(media != nullptr, "Expected media interface.");
            action(std::ref(*media));
        }
    }

    template <typename T, typename Reducer>
    void reduceMediaInto(T&& init, Reducer reducer)
    {
        for (const auto media : media_array_)
        {
            CETL_DEBUG_ASSERT(media != nullptr, "Expected media interface.");
            reducer(std::forward<T>(init), std::ref(*media));
        }
    }

    template <typename T, typename Reducer>
    CETL_NODISCARD T reduceMedia(const T init, Reducer reducer) const
    {
        T acc = init;
        for (const auto media : media_array_)
        {
            CETL_DEBUG_ASSERT(media != nullptr, "Expected media interface.");
            acc = reducer(std::forward<T>(acc), std::ref(*media));
        }
        return acc;
    }

    CETL_NODISCARD cetl::optional<AnyError> ensureNewSessionFor(const CanardTransferKind transfer_kind,
                                                                const PortId             port_id,
                                                                const PortId             max_port_id) noexcept
    {
        if (port_id > max_port_id)
        {
            return ArgumentError{};
        }

        const auto hasSubscription = canardRxHasSubscription(&canard_instance_, transfer_kind, port_id);
        if (hasSubscription < 0)
        {
            return anyErrorFromCanard(hasSubscription);
        }
        if (hasSubscription > 0)
        {
            return SessionAlreadyExistsError{};
        }

        return {};
    }

    // MARK: Data members:

    const libcyphal::detail::VarArray<IMedia*> media_array_;

};  // TransportImpl

}  // namespace detail

CETL_NODISCARD inline Expected<UniquePtr<ICanTransport>, FactoryError> makeTransport(
    cetl::pmr::memory_resource&                    memory,
    IMultiplexer&                                  multiplexer,
    const std::array<IMedia*, MaxMediaInterfaces>& media,  // TODO: replace with `cetl::span<IMedia*>`
    const cetl::optional<NodeId>                   local_node_id)
{
    return detail::TransportImpl::make(memory, multiplexer, media, local_node_id);
}

}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_TRANSPORT_HPP_INCLUDED
