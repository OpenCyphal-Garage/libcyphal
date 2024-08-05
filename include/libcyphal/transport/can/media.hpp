/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_CAN_MEDIA_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_MEDIA_HPP_INCLUDED

#include "libcyphal/transport/errors.hpp"
#include "libcyphal/types.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>

#include <cstddef>
#include <cstdint>

namespace libcyphal
{
namespace transport
{
namespace can
{

using CanId = std::uint32_t;

struct Filter final
{
    CanId id{};
    CanId mask{};
};
using Filters = cetl::span<const Filter>;

/// @brief Defines interface to a custom CAN bus media implementation.
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

    /// @brief Get the maximum transmission unit (MTU) of the CAN bus.
    ///
    /// This value may change arbitrarily at runtime. The transport implementation will query it before every
    /// transmission on the port. This value has no effect on the reception pipeline as it can accept arbitrary MTU.
    ///
    virtual std::size_t getMtu() const noexcept = 0;

    /// @brief Set the filters for the CAN bus.
    ///
    /// If there are fewer hardware filters available than requested, the configuration will be coalesced as described
    /// in the Cyphal/CAN Specification. If zero filters are requested, all incoming traffic will be rejected.
    /// While reconfiguration is in progress, incoming frames may be lost and/or unwanted frames may be received.
    /// The lifetime of the filter array may end upon return (no references retained).
    ///
    /// @return `nullopt` on success; otherwise some `MediaError` in case of a low-level error.
    ///         In case of any media failure, the transport will try apply filters again on its next run.
    ///
    virtual cetl::optional<MediaFailure> setFilters(const Filters filters) noexcept = 0;

    /// @brief Schedules the frame for transmission asynchronously and return immediately.
    ///
    /// @param deadline The deadline for the push operation. Media implementation should drop the payload
    ///                 if the deadline is exceeded (aka `now > deadline`).
    /// @param can_id The destination CAN ID of the frame.
    /// @return `true` if the frame is accepted or already timed out;
    ///         `false` to try again later (f.e. b/c output TX queue is currently full).
    ///         If any media failure occurred, the frame will be dropped by transport.
    ///@{
    struct PushResult
    {
        struct Success
        {
            bool is_accepted;
        };
        using Failure = MediaFailure;

        using Type = Expected<Success, Failure>;
    };
    virtual PushResult::Type push(const TimePoint                    deadline,
                                  const CanId                        can_id,
                                  const cetl::span<const cetl::byte> payload) noexcept = 0;
    ///@}

    /// @brief Takes the next payload fragment (aka CAN frame) from the reception queue unless it's empty.
    ///
    /// @param payload_buffer The payload of the frame will be written into the mutable `payload_buffer` (aka span).
    /// @return Description of a received fragment if available; otherwise an empty optional is returned immediately.
    ///         `nodiscard` is used to prevent ignoring the return value, which contains not only possible media error,
    ///         but also important metadata (like `payload_size` field for further parsing of the result payload).
    ///@{
    struct PopResult
    {
        struct Metadata
        {
            TimePoint   timestamp;
            CanId       can_id{};
            std::size_t payload_size{};
        };
        using Success = cetl::optional<Metadata>;
        using Failure = MediaFailure;

        using Type = Expected<Success, Failure>;
    };
    CETL_NODISCARD virtual PopResult::Type pop(const cetl::span<cetl::byte> payload_buffer) noexcept = 0;
    ///@}

    /// @brief Registers "ready to push" callback function at a given executor.
    ///
    /// The callback will be called by the executor when this CAN media will be ready to accept more data.
    ///
    /// @param executor The executor to register the callback at.
    /// @param function The function to be called when CAN media became "ready to push".
    /// @return Type-erased instance of the registered callback. Instance must not outlive the executor,
    ///         and must be used only with the same executor; otherwise undefined behavior.
    ///
    CETL_NODISCARD virtual IExecutor::Callback::Any registerPushCallback(IExecutor&                      executor,
                                                                         IExecutor::Callback::Function&& function) = 0;

    /// @brief Registers "ready to pop" callback function at a given executor.
    ///
    /// The callback will be called by the executor when this CAN media has new data to read.
    ///
    /// @param executor The executor to register the callback at.
    /// @param function The function to be called when CAN media became "ready to pop".
    /// @return Type-erased instance of the registered callback. Instance must not outlive the executor,
    ///         and must be used only with the same executor; otherwise undefined behavior.
    ///
    CETL_NODISCARD virtual IExecutor::Callback::Any registerPopCallback(IExecutor&                      executor,
                                                                        IExecutor::Callback::Function&& function) = 0;

protected:
    IMedia()  = default;
    ~IMedia() = default;

};  // IMedia

}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_MEDIA_HPP_INCLUDED
