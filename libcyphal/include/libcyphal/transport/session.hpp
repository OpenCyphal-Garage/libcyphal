/// @file
/// Session objects for Transports.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef LIBCYPHAL_TRANSPORT_SESSION_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_SESSION_HPP_INCLUDED

#include <chrono>
#include <functional>

#include "libcyphal/libcyphal.hpp"
#include "libcyphal/janky.hpp"
#include "libcyphal/transport/transfer.hpp"
#include "libcyphal/transport/data_specifier.hpp"
#include "libcyphal/transport/payload_metadata.hpp"

namespace libcyphal
{
namespace transport
{

// +---------------------------------------------------------------------------+
/// This data-class models the session specifier [4.1.1.6 Session Specifier] of the Cyphal Specification except that we
/// assume that one end of the session terminates at the local node. There are specializations for input and output
/// sessions with additional logic, but they do not add extra data (because remember this class follows the protocol
/// model definition).
class SessionSpecifier
{
protected:
    SessionSpecifier(DataSpecifier data_specifier, janky::optional<NodeID> remote_node_id)
        : data_specifier_{data_specifier}
        , remote_node_id_{remote_node_id}
    {
    }

public:
    ~SessionSpecifier() noexcept = default;
    SessionSpecifier(const SessionSpecifier& rhs) noexcept
        : data_specifier_{rhs.data_specifier_}
        , remote_node_id_{rhs.remote_node_id_}
    {
    }

public:
    SessionSpecifier(SessionSpecifier&& rhs) noexcept
        : data_specifier_{std::move(rhs.data_specifier_)}
        , remote_node_id_{std::move(rhs.remote_node_id_)}
    {
    }
    SessionSpecifier& operator=(SessionSpecifier&&)      = delete;
    SessionSpecifier& operator=(const SessionSpecifier&) = delete;

    /// See `libcyphal::transport::DataSpecifier`.
    const DataSpecifier& getDataSpecifier() const noexcept
    {
        return data_specifier_;
    }

    /// If not None: output sessions are unicast to that node-ID, and input sessions ignore all transfers
    /// except those that originate from the specified remote node-ID.
    /// If None: output sessions are broadcast and input sessions are promiscuous.
    janky::optional<NodeID> getRemoteNodeId() const noexcept
    {
        return remote_node_id_;
    }

    std::size_t hash() const noexcept
    {
        return data_specifier_.hash() ^ (std::hash<NodeID>{}(remote_node_id_) << 1);
    }

    bool operator==(const SessionSpecifier& rhs) const noexcept
    {
        return (data_specifier_ == rhs.data_specifier_) && (remote_node_id_ == rhs.remote_node_id_);
    }
private:
    DataSpecifier           data_specifier_;
    janky::optional<NodeID> remote_node_id_;
};

// +---------------------------------------------------------------------------+
/// If the remote node-ID is set, this is a selective session (accept data from the specified remote node only);
/// otherwise this is a promiscuous session (accept data from any node).
class InputSessionSpecifier : public SessionSpecifier
{
public:
    InputSessionSpecifier(DataSpecifier data_specifier, janky::optional<NodeID> remote_node_id = janky::nullopt)
        : SessionSpecifier{data_specifier, remote_node_id}
        , is_promiscuous_{(!remote_node_id) ? true : false}
    {
    }
    ~InputSessionSpecifier() noexcept = default;
    InputSessionSpecifier(const InputSessionSpecifier& rhs) noexcept
        : SessionSpecifier{rhs}
    {
    }
    InputSessionSpecifier(InputSessionSpecifier&& rhs)
        : SessionSpecifier{std::move(rhs)}
    {
    }
    InputSessionSpecifier& operator=(InputSessionSpecifier&&)      = delete;
    InputSessionSpecifier& operator=(const InputSessionSpecifier&) = delete;

    bool isPromiscuous() const noexcept
    {
        return is_promiscuous_;
    }

    bool operator==(const InputSessionSpecifier& rhs) const noexcept
    {
        return SessionSpecifier::operator==(rhs);
    }

private:
    bool is_promiscuous_;
};

// +---------------------------------------------------------------------------+

/// If the remote node-ID is set, this is a unicast session (use unicast transfers);
/// otherwise this is a broadcast session (use broadcast transfers).
/// The Specification v1.0 allows the following kinds of transfers:
///
/// - Broadcast message transfers.
/// - Unicast service transfers.
///
/// Anything else is invalid per Cyphal v1.0.
/// A future version of the specification may add support for unicast messages for at least some transports.
/// Here, we go ahead and assume that unicast message transfers are valid in general;
/// it is up to a particular transport implementation to choose whether they are supported.
/// Beware that this is a non-standard experimental protocol extension and it may be removed
/// depending on how the next versions of the Specification evolve.
/// You can influence that by leaving feedback at https://forum.opencyphal.org.
///
/// To summarize:
///
/// +--------------------+--------------------------------------+---------------------------------------+
/// |                    | Unicast                              | Broadcast                             |
/// +====================+======================================+=======================================+
/// | **Message**        | Experimental, may be allowed in v1.x | Allowed by Specification              |
/// +--------------------+--------------------------------------+---------------------------------------+
/// | **Service**        | Allowed by Specification             | Banned by Specification               |
/// +--------------------+--------------------------------------+---------------------------------------+
class OutputSessionSpecifier : public SessionSpecifier
{
public:
    OutputSessionSpecifier(DataSpecifier data_specifier, janky::optional<NodeID> remote_node_id = janky::nullopt )
        : SessionSpecifier{data_specifier, remote_node_id}
        , is_broadcast_{(!remote_node_id) ? true : false}
    {
        CETL_DEBUG_ASSERT((data_specifier.getRole() == DataSpecifier::Role::Message) || remote_node_id,
                          "Service transfers shall be unicast");
    }

    ~OutputSessionSpecifier() noexcept = default;
    OutputSessionSpecifier(const OutputSessionSpecifier& rhs) noexcept
        : SessionSpecifier{rhs}
    {
    }
    OutputSessionSpecifier(OutputSessionSpecifier&& rhs)
        : SessionSpecifier{std::move(rhs)}
    {
    }
    OutputSessionSpecifier& operator=(OutputSessionSpecifier&&)      = delete;
    OutputSessionSpecifier& operator=(const OutputSessionSpecifier&) = delete;

    bool isBroadcast() const noexcept
    {
        return is_broadcast_;
    }

    bool operator==(const OutputSessionSpecifier& rhs) const noexcept
    {
        return SessionSpecifier::operator==(rhs);
    }

private:
    bool is_broadcast_;
};

// +---------------------------------------------------------------------------+

/// Abstract session base class. This is further specialized by input and output.
class ISession
{
public:
    virtual SessionSpecifier getSpecifier() const noexcept       = 0;
    virtual PayloadMetadata  getPayloadMetadata() const noexcept = 0;

    /// After a session is closed, none of its methods can be used.
    /// Methods invoked on a closed session should return `libcyphal::ResultCode::ResourceClosedError`.
    /// Subsequent calls to close() will have no effect (no exception either).
    ///
    /// Methods where a task is blocked (such as receive()) at the time of close() will exit with
    /// `libcyphal::ResultCode::ResourceClosedError`.
    virtual void close() noexcept = 0;

protected:
    ~ISession() noexcept = default;
};

class InputTransfer
{
public:
    TransferFrom from_;
    void* opaque_data_;
};

// +---------------------------------------------------------------------------+

/// Either promiscuous or selective input session.
/// The configuration cannot be changed once instantiated.
///
/// Users shall never construct instances themselves;
/// instead, the factory method `libcyphal::transport::Transport.get_input_session()` shall be used.
class IInputSession : public virtual ISession
{
public:
    /// Attempts to receive the transfer before the deadline [second].
    /// Returns None if the transfer is not received before the deadline.
    /// The deadline is compared against `asyncio.AbstractEventLoop.time`.
    /// If the deadline is in the past, checks once if there is a transfer and then returns immediately
    /// without context switching.
    ///
    /// Implementations that use internal queues are recommended to permit the consumer to continue reading
    /// queued transfers after the instance is closed until the queue is empty.
    /// In other words, it is recommended to not raise the ResourceClosed exception until
    /// the instance is closed AND the queue is empty.
    virtual Status receive(TransferFrom& out_transfer) = 0;

    /// By default, the transfer-ID timeout [second] is initialized with the default value provided in the
    /// Cyphal specification.
    /// It can be overridden using this interface if necessary (rarely is).
    /// An attempt to assign an invalid timestamp value raises :class:`ValueError`.
    virtual std::chrono::milliseconds getTransferIdTimeout() const noexcept = 0;

    virtual Status setTransferIdTimeout(std::chrono::milliseconds value) = 0;

protected:
    ~IInputSession() noexcept = default;
};

// +---------------------------------------------------------------------------+

/// Either broadcast or unicast output session.
/// The configuration cannot be changed once instantiated.
///
/// Users shall never construct instances themselves;
/// instead, the factory method `libcyphal::transport::Transport::get_output_session()` shall be used.
class IOutputSession : public virtual ISession
{
public:

    /// Sends the transfer; blocks if necessary until the specified deadline [second].
    /// The deadline value is compared against :meth:`asyncio.AbstractEventLoop.time`.
    /// Returns when transmission is completed, in which case the return value is True;
    /// or when the deadline is reached, in which case the return value is False.
    /// In the case of timeout, a multi-frame transfer may be emitted partially,
    /// thereby rendering the receiving end unable to process it.
    /// If the deadline is in the past, the method attempts to send the frames anyway as long as that
    /// doesn't involve blocking (i.e., task context switching).

    /// Some transports or media sub-layers may be unable to guarantee transmission strictly before the deadline;
    /// for example, that may be the case if there is an additional buffering layer under the transport/media
    /// implementation (e.g., that could be the case with SLCAN-interfaced CAN bus adapters, IEEE 802.15.4 radios,
    /// and so on, where the data is pushed through an intermediary interface and briefly buffered again before
    /// being pushed onto the media).
    /// This is a design limitation imposed by the underlying non-real-time platform that Python runs on;
    /// it is considered acceptable since PyCyphal is designed for soft-real-time applications at most.
    virtual Status send(const Transfer& transfer, TransferPriority priority, std::chrono::milliseconds monotonic_deadline) = 0;

protected:
    ~IOutputSession() noexcept = default;
};

}  // namespace transport
}  // namespace libcyphal

namespace std
{
template<>
struct std::hash<libcyphal::transport::InputSessionSpecifier>
{
    std::size_t operator()(libcyphal::transport::InputSessionSpecifier const& s) const noexcept
    {
        return s.hash();
    }
};

template<>
struct std::hash<libcyphal::transport::OutputSessionSpecifier>
{
    std::size_t operator()(libcyphal::transport::OutputSessionSpecifier const& s) const noexcept
    {
        return s.hash();
    }
};
} // namespace std

#endif  // LIBCYPHAL_TRANSPORT_SESSION_HPP_INCLUDED
