/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_UDP_MEDIA_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_UDP_MEDIA_HPP_INCLUDED

#include "tx_rx_sockets.hpp"

#include "libcyphal/transport/errors.hpp"
#include "libcyphal/types.hpp"

#include <cetl/pf17/cetlpf.hpp>

namespace libcyphal
{
namespace transport
{
namespace udp
{

/// @brief Defines interface to a custom UDP media implementation.
///
/// Implementation is supposed to be provided by an user of the library.
///
class IMedia
{
public:
    IMedia(const IMedia&)                = delete;
    IMedia(IMedia&&) noexcept            = delete;
    IMedia& operator=(const IMedia&)     = delete;
    IMedia& operator=(IMedia&&) noexcept = delete;

    /// Constructs a new TX socket bound to this media.
    ///
    /// It's called by the transport layer (per each such media) on attempt to create a new TX session.
    /// The transport layer will use the returned socket instance to send messages or service requests/responses.
    /// The socket instance (once successfully created) is stored inside of the transport layer,
    /// shared for ALL current and future TX sessions, and will be released when the transport layer is destroyed.
    /// As a result, the total number of TX sockets is limited by the redundancy factor of the media.
    /// Releasing of a TX session will NOT release corresponding shared TX sockets -
    /// they will be all released when the whole transport layer is destroyed.
    ///
    /// Described above "adhoc" socket creation and sharing mechanism is used also in case of failures. Namely,
    /// if this method fails, the transport layer will report the error to the user's transient error handler,
    /// which in turn might decide that this error condition is a "transient" one indeed,
    /// and so deserves either a retry logic or might just lean on other redundant media for transmission.
    /// In this case, the transport layer will still successfully create a new TX session, even if some
    /// of the redundant media TX sockets are missing (aka "faulty"), and will try to re-create them every time
    /// there is something valid (not expired by timeout) to send to the media.
    ///
    ///@{
    struct MakeTxSocketResult
    {
        using Success = UniquePtr<ITxSocket>;
        using Failure = cetl::variant<MemoryError, PlatformError>;

        using Type = Expected<Success, Failure>;
    };
    virtual MakeTxSocketResult::Type makeTxSocket() = 0;
    ///@}

    /// Constructs a new RX socket bound to the specified multicast group endpoint.
    ///
    /// It's called by the transport layer (per each such media) on attempt to create a new RX session.
    /// The transport layer will use returned socket instance to receive messages or service requests/responses.
    /// In contrast to TX sockets (described above for the `makeTxSocket` method), sharing strategy of RX sockets
    /// heavily depends on whether RX socket is made for messages or services:
    ///
    /// - For messages, the transport layer will create a new RX socket for each new message session.
    ///   As a result, the total number of message RX sockets is limited by the number of message sessions (`M`)
    ///   multiplied by the redundancy factor (`R`) of the media, aka `M * R`. Releasing of a message RX session
    ///   will also release its corresponding RX sockets.
    ///
    /// - For services, the transport layer will use similar to the TX sockets sharing strategy,
    ///   i.e. it will create it once, store and reuse it for ALL current and future service receptions.
    ///   As a result, the total number of service RX sockets is limited by the redundancy factor of the media.
    ///   Releasing of a service RX session will NOT release corresponding shared RX sockets -
    ///   they will be all released when the whole transport layer is destroyed.
    ///
    /// Described above "adhoc" socket creation and sharing mechanism is used also in case of failures. Namely,
    /// if this method fails, the transport layer will report the error to the user's transient error handler,
    /// which in turn might decide that this error condition is a "transient" one indeed,
    /// and so deserves either a retry logic or might just lean on other redundant media for reception.
    /// In this case, the transport layer will still successfully create a new RX session, even if some
    /// of the redundant media RX sockets are missing (aka "faulty"), and transport will try to re-create them
    /// on each reception run. It's up to user's `IMedia` implementation to decide whether to actually try re-create
    /// RX socket on each attempt, make it occasionally, or just ignore the error and lean on other redundant media.
    ///
    ///@{
    struct MakeRxSocketResult
    {
        using Success = UniquePtr<IRxSocket>;
        using Failure = cetl::variant<MemoryError, PlatformError, ArgumentError>;

        using Type = Expected<Success, Failure>;
    };
    virtual MakeRxSocketResult::Type makeRxSocket(const IpEndpoint& multicast_endpoint) = 0;
    ///@}

protected:
    IMedia()  = default;
    ~IMedia() = default;

};  // IMedia

}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_UDP_MEDIA_HPP_INCLUDED
