/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_UDP_TRANSPORT_IMPL_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_TRANSPORT_IMPL_HPP_INCLUDED

#include "delegate.hpp"
#include "media.hpp"
#include "msg_rx_session.hpp"
#include "msg_tx_session.hpp"
#include "svc_rx_sessions.hpp"
#include "svc_tx_sessions.hpp"
#include "udp_transport.hpp"

#include "libcyphal/runnable.hpp"
#include "libcyphal/transport/errors.hpp"
#include "libcyphal/transport/msg_sessions.hpp"
#include "libcyphal/transport/multiplexer.hpp"
#include "libcyphal/transport/svc_sessions.hpp"
#include "libcyphal/transport/transport.hpp"
#include "libcyphal/transport/types.hpp"
#include "libcyphal/types.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>
#include <udpard.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace libcyphal
{
namespace transport
{
namespace udp
{

/// Internal implementation details of the UDP transport.
/// Not supposed to be used directly by the users of the library.
///
namespace detail
{

class TransportImpl final : private TransportDelegate, public IUdpTransport
{
    /// @brief Defines specification for making interface unique ptr.
    ///
    struct Spec
    {
        using Interface = IUdpTransport;
        using Concrete  = TransportImpl;

        // In use to disable public construction.
        // See https://seanmiddleditch.github.io/enabling-make-unique-with-private-constructors/
        explicit Spec() = default;
    };

    /// @brief Internal (private) storage of a media index, its interface and TX queue.
    ///
    struct Media final
    {
    public:
        Media(const std::size_t                 index,
              IMedia&                           interface,
              const UdpardNodeID* const         local_node_id,
              const std::size_t                 tx_capacity,
              const struct UdpardMemoryResource udp_mem_res)
            : index_{static_cast<std::uint8_t>(index)}
            , interface_{interface}
            , udpard_tx_{}
        {
            const std::int8_t result = ::udpardTxInit(&udpard_tx_, local_node_id, tx_capacity, udp_mem_res);
            CETL_DEBUG_ASSERT(result == 0, "There should be no path for an error here.");
            (void) result;
        }

        std::uint8_t index() const
        {
            return index_;
        }

        IMedia& interface() const
        {
            return interface_;
        }

        UdpardTx& udpard_tx()
        {
            return udpard_tx_;
        }

        void propagateMtuToTxQueue()
        {
            udpard_tx_.mtu = interface_.getMtu();
        }

    private:
        const std::uint8_t index_;
        IMedia&            interface_;
        UdpardTx           udpard_tx_;
    };
    using MediaArray = libcyphal::detail::VarArray<Media>;

public:
    CETL_NODISCARD static Expected<UniquePtr<IUdpTransport>, FactoryError> make(const MemoryResourcesSpec& mem_res_spec,
                                                                                IMultiplexer&              multiplexer,
                                                                                const cetl::span<IMedia*>  media,
                                                                                const std::size_t          tx_capacity)
    {
        // Verify input arguments:
        // - At least one media interface must be provided, but no more than the maximum allowed (255).
        //
        const auto media_count =
            static_cast<std::size_t>(std::count_if(media.begin(), media.end(), [](const IMedia* const media_ptr) {
                return media_ptr != nullptr;
            }));
        if ((media_count == 0) || (media_count > std::numeric_limits<std::uint8_t>::max()))
        {
            return ArgumentError{};
        }

        const MemoryResources memory_resources{mem_res_spec.general,
                                               makeUdpardMemoryResource(mem_res_spec.session, mem_res_spec.general),
                                               makeUdpardMemoryResource(mem_res_spec.fragment, mem_res_spec.general),
                                               makeUdpardMemoryDeleter(mem_res_spec.payload, mem_res_spec.general)};

        const UdpardNodeID unset_node_id = UDPARD_NODE_ID_UNSET;

        // False positive of clang-tidy - we move `media_array` to the `transport` instance, so can't make it const.
        // NOLINTNEXTLINE(misc-const-correctness)
        MediaArray media_array = makeMediaArray(mem_res_spec.general,
                                                media_count,
                                                media,
                                                &unset_node_id,
                                                tx_capacity,
                                                memory_resources.fragment);
        if (media_array.size() != media_count)
        {
            return MemoryError{};
        }

        auto transport = libcyphal::detail::makeUniquePtr<Spec>(memory_resources.general,
                                                                Spec{},
                                                                memory_resources,
                                                                multiplexer,
                                                                std::move(media_array));
        if (transport == nullptr)
        {
            return MemoryError{};
        }

        return transport;
    }

    TransportImpl(Spec, const MemoryResources& memory_resources, IMultiplexer& multiplexer, MediaArray&& media_array)
        : TransportDelegate{memory_resources}
        , media_array_{std::move(media_array)}
        , local_node_id_{UDPARD_NODE_ID_UNSET}
    {
        for (auto& media : media_array_)
        {
            media.udpard_tx().local_node_id = &local_node_id_;
        }

        // TODO: Use it!
        (void) multiplexer;
    }

private:
    // MARK: IUdpTransport

    // MARK: ITransport

    CETL_NODISCARD cetl::optional<NodeId> getLocalNodeId() const noexcept override
    {
        if (local_node_id_ > UDPARD_NODE_ID_MAX)
        {
            return cetl::nullopt;
        }

        return cetl::make_optional(local_node_id_);
    }

    CETL_NODISCARD cetl::optional<ArgumentError> setLocalNodeId(const NodeId node_id) noexcept override
    {
        // Allow setting the same node ID multiple times, but only once otherwise.
        //
        if (local_node_id_ == node_id)
        {
            return cetl::nullopt;
        }
        if (local_node_id_ != UDPARD_NODE_ID_UNSET)
        {
            return ArgumentError{};
        }

        local_node_id_ = node_id;

        return cetl::nullopt;
    }

    CETL_NODISCARD ProtocolParams getProtocolParams() const noexcept override
    {
        std::size_t min_mtu = std::numeric_limits<std::size_t>::max();
        for (const Media& media : media_array_)
        {
            min_mtu = std::min(min_mtu, media.interface().getMtu());
        }

        // TODO: What about `transfer_id_modulo`???
        return ProtocolParams{0, min_mtu, UDPARD_NODE_ID_MAX + 1};
    }

    CETL_NODISCARD Expected<UniquePtr<IMessageRxSession>, AnyError> makeMessageRxSession(
        const MessageRxParams& params) override
    {
        // TODO: Uncomment!
        //        const cetl::optional<AnyError> any_error = ensureNewSessionFor(CanardTransferKindMessage,
        //        params.subject_id); if (any_error.has_value())
        //        {
        //            return any_error.value();
        //        }

        return MessageRxSession::make(asDelegate(), params);
    }

    CETL_NODISCARD Expected<UniquePtr<IMessageTxSession>, AnyError> makeMessageTxSession(
        const MessageTxParams& params) override
    {
        return MessageTxSession::make(asDelegate(), params);
    }

    CETL_NODISCARD Expected<UniquePtr<IRequestRxSession>, AnyError> makeRequestRxSession(
        const RequestRxParams& params) override
    {
        // TODO: Uncomment!
        //        const cetl::optional<AnyError> any_error = ensureNewSessionFor(CanardTransferKindRequest,
        //        params.service_id); if (any_error.has_value())
        //        {
        //            return any_error.value();
        //        }

        return SvcRequestRxSession::make(asDelegate(), params);
    }

    CETL_NODISCARD Expected<UniquePtr<IRequestTxSession>, AnyError> makeRequestTxSession(
        const RequestTxParams& params) override
    {
        return SvcRequestTxSession::make(asDelegate(), params);
    }
    CETL_NODISCARD Expected<UniquePtr<IResponseRxSession>, AnyError> makeResponseRxSession(
        const ResponseRxParams& params) override
    {
        // TODO: Uncomment!
        //        const cetl::optional<AnyError> any_error = ensureNewSessionFor(CanardTransferKindResponse,
        //        params.service_id); if (any_error.has_value())
        //        {
        //            return any_error.value();
        //        }

        return SvcResponseRxSession::make(asDelegate(), params);
    }
    CETL_NODISCARD Expected<UniquePtr<IResponseTxSession>, AnyError> makeResponseTxSession(
        const ResponseTxParams& params) override
    {
        return SvcResponseTxSession::make(asDelegate(), params);
    }

    // MARK: IRunnable

    CETL_NODISCARD IRunnable::MaybeError run(const TimePoint) override
    {
        return AnyError{NotImplementedError{}};
    }

    CETL_NODISCARD TransportDelegate& asDelegate()
    {
        return *this;
    }

    // MARK: Privates:

    CETL_NODISCARD static MediaArray makeMediaArray(cetl::pmr::memory_resource&       memory,
                                                    const std::size_t                 media_count,
                                                    const cetl::span<IMedia*>         media_interfaces,
                                                    const UdpardNodeID* const         local_node_id_,
                                                    const std::size_t                 tx_capacity,
                                                    const struct UdpardMemoryResource udp_mem_res)
    {
        MediaArray media_array{media_count, &memory};

        // Reserve the space for the whole array (to avoid reallocations).
        // Capacity will be less than requested in case of out of memory.
        media_array.reserve(media_count);
        if (media_array.capacity() >= media_count)
        {
            std::size_t index = 0;
            for (IMedia* const media_interface : media_interfaces)
            {
                if (media_interface != nullptr)
                {
                    IMedia& media = *media_interface;
                    media_array.emplace_back(index, media, local_node_id_, tx_capacity, udp_mem_res);
                    index++;
                }
            }
            CETL_DEBUG_ASSERT(index == media_count, "");
            CETL_DEBUG_ASSERT(media_array.size() == media_count, "");
        }

        return media_array;
    }

    // MARK: Data members:

    MediaArray   media_array_;
    UdpardNodeID local_node_id_;

};  // TransportImpl

}  // namespace detail

/// @brief Makes a new UDP transport instance.
///
/// NB! Lifetime of the transport instance must never outlive memory resources, `media` and `multiplexer` instances.
///
/// @param mem_res_spec Specification of polymorphic memory resources to use for all allocations.
/// @param multiplexer Interface of the multiplexer to use.
/// @param media Collection of redundant media interfaces to use.
/// @param tx_capacity Total number of frames that can be queued for transmission per `IMedia` instance.
/// @return Unique pointer to the new UDP transport instance or an error.
///
inline Expected<UniquePtr<IUdpTransport>, FactoryError> makeTransport(const MemoryResourcesSpec& mem_res_spec,
                                                                      IMultiplexer&              multiplexer,
                                                                      const cetl::span<IMedia*>  media,
                                                                      const std::size_t          tx_capacity)
{
    return detail::TransportImpl::make(mem_res_spec, multiplexer, media, tx_capacity);
}
}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_TRANSPORT_IMPL_HPP_INCLUDED
