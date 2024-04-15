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
class TransportImpl final : public ICanTransport
{
public:
    TransportImpl(cetl::pmr::memory_resource&            memory,
                  libcyphal::detail::VarArray<IMedia*>&& media_array,
                  const CanardNodeID                     canard_node_id)
        : memory_{memory}
        , media_array_{std::move(media_array)}
        , canard_instance_{canardInit(canardMemoryAllocate, canardMemoryFree)}
    {
        canard_instance_.user_reference = this;
        canard_instance_.node_id        = canard_node_id;
    }

    // MARK: ICanTransport

    // MARK: ITransport

    CETL_NODISCARD cetl::optional<NodeId> getLocalNodeId() const noexcept override
    {
        return canard_instance_.node_id > CANARD_NODE_ID_MAX ? cetl::nullopt
                                                             : cetl::make_optional(canard_instance_.node_id);
    }
    CETL_NODISCARD ProtocolParams getProtocolParams() const noexcept override
    {
        const auto min_mtu = reduceMediaInto(std::numeric_limits<std::size_t>::max(),
                                             [](auto&& mtu, IMedia& media) { mtu = std::min(mtu, media.getMtu()); });
        return ProtocolParams{1 << CANARD_TRANSFER_ID_BIT_LENGTH, min_mtu, CANARD_NODE_ID_MAX + 1};
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

    // MARK: IRunnable

    void run(const TimePoint) override {}

private:
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
    T reduceMediaInto(T&& init, Reducer reducer) const
    {
        for (const auto media : media_array_)
        {
            CETL_DEBUG_ASSERT(media != nullptr, "Expected media interface.");
            reducer(std::forward<T>(init), std::ref(*media));
        }
        return init;
    }

    // MARK: Data members:

    cetl::pmr::memory_resource&                memory_;
    const libcyphal::detail::VarArray<IMedia*> media_array_;
    CanardInstance                             canard_instance_;

};  // TransportImpl

}  // namespace detail

CETL_NODISCARD inline Expected<UniquePtr<ICanTransport>, FactoryError> makeTransport(
    cetl::pmr::memory_resource&                    memory,
    IMultiplexer&                                  multiplexer,
    const std::array<IMedia*, MaxMediaInterfaces>& media,  // TODO: replace with `cetl::span<IMedia*>`
    const cetl::optional<NodeId>                   local_node_id)
{
    // TODO: Use these!
    (void) multiplexer;

    // Verify input arguments:
    // - At least one media interface must be provided.
    // - If a local node ID is provided, it must be within the valid range.
    //
    const auto media_count =
        static_cast<std::size_t>(std::count_if(media.cbegin(), media.cend(), [](auto m) { return m != nullptr; }));
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
    std::copy_if(media.cbegin(), media.cend(), std::back_inserter(media_array), [](auto m) { return m != nullptr; });
    CETL_DEBUG_ASSERT(!media_array.empty() && (media_array.size() == media_count), "");

    const auto canard_node_id = static_cast<CanardNodeID>(local_node_id.value_or(CANARD_NODE_ID_UNSET));

    libcyphal::detail::PmrAllocator<detail::TransportImpl> allocator{&memory};
    auto transport = cetl::pmr::Factory::make_unique(allocator, memory, std::move(media_array), canard_node_id);

    return UniquePtr<ICanTransport>{transport.release(), UniquePtr<ICanTransport>::deleter_type{allocator, 1}};
}

}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_TRANSPORT_HPP_INCLUDED
