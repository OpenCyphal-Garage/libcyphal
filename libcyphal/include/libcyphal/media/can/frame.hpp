/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Common CAN frame definition
/// @todo Move the 'media' folder under transport, update namespacing, etc.

#ifndef LIBCYPHAL_MEDIA_CAN_FRAME_HPP_INCLUDED
#define LIBCYPHAL_MEDIA_CAN_FRAME_HPP_INCLUDED

#include <array>
#include <cstring>
#include <functional>
#include <utility>
#include "libcyphal/media/can/dataLengthCode.hpp"
#include "libcyphal/media/can/identifier.hpp"
#include "libcyphal/media/can/filter.hpp"
#include "libcyphal/types/time.hpp"

namespace libcyphal
{
namespace media
{
namespace can
{

/// Type used to pass CAN frame data into and out of CAN drivers.
/// This contains all CAN frame data that is relevant to an application and hides frame
/// fields that are handled by a lower level, such as the CRC field.
/// @todo remove or limit use of templates
template <typename ID_TYPE>
class Frame
{
public:
    /// The maximum number of data in a frame
    constexpr static std::size_t MaxDataPayload = ID_TYPE::MaxDataPayload;

    static_assert(MaxDataPayload == standard::MaxDataPayload or MaxDataPayload == extended::MaxDataPayload,
                  "Only Standard or Extended is allowed");

    /// Default constructor - sets default frame to extended for backward-compatibility
    constexpr Frame() noexcept
        : received_timestamp_{0}
        , id_{0}
        , frames_lost_{0}
        , received_timestamp_valid_{false}
        , dlc_{0}
        , data_{}
    {
    }

    /// Simple Constructor (This can be performed completely at compile time)
    constexpr Frame(ID_TYPE id, DataLengthCode dlc, std::initializer_list<uint8_t> data) noexcept
        : received_timestamp_{0}
        , id_{id}
        , frames_lost_{0}
        , received_timestamp_valid_{false}
        , dlc_{dlc}
        , data_{}
    {
        (void) data;
        if /* constexpr */ (MaxDataPayload == standard::MaxDataPayload)
        {
            dlc_.clampToStandard();
        }
        size_t i = 0ul;
        for (auto& item : data_)
        {
            if (i < dlc_.toLength())
            {
                data_[i++] = item;
            }
        }
    }

    /// Simple Constructor (This can be performed completely at compile time)
    constexpr Frame(ID_TYPE id, DataLengthCode dlc, const uint8_t list[]) noexcept
        : received_timestamp_{}
        , id_{id}
        , frames_lost_{0}
        , received_timestamp_valid_{false}
        , dlc_{dlc}
        , data_{}
    {
        if /* constexpr */ (MaxDataPayload == standard::MaxDataPayload)
        {
            dlc_.clampToStandard();
        }
        for (size_t i = 0ul; i < dlc_.toLength(); i++)
        {
            data_[i] = list[i];
        }
    }

    /// Defines a copy constructor from another type of Frame
    /// @note These are the specialized copiers. If the copy constructor isn't defined, it's not allowed.
    template <typename OTHER_TYPE>
    Frame(const OTHER_TYPE& other);

    /// Copy Construction
    constexpr Frame(const Frame& other) noexcept
        : received_timestamp_{other.received_timestamp_}
        , id_{other.id_}
        , frames_lost_{other.frames_lost_}
        , dlc_{other.dlc_}
        , data_{}
    {
        memcpy(data_, other.data_, dlc_.toLength());
    }

    /// Copy Assignment
    Frame& operator=(const Frame& other) noexcept
    {
        received_timestamp_ = other.received_timestamp_;
        id_                 = other.id_;
        frames_lost_        = other.frames_lost_;
        dlc_                = other.dlc_;
        memcpy(data_, other.data_, dlc_.toLength());
        return (*this);
    }

    /// Move Construction
    constexpr Frame(Frame&& other) noexcept
        : received_timestamp_{other.received_timestamp_}
        , id_{other.id_}
        , frames_lost_{other.frames_lost_}
        , dlc_{other.dlc_}
        , data_{}
    {
        memcpy(data_, other.data_, dlc_.toLength());
    }

    /// Move Assign (mainly used by unit tests)
    Frame& operator=(Frame&& other) noexcept
    {
        received_timestamp_ = other.received_timestamp_;
        id_                 = other.id_;
        frames_lost_        = other.frames_lost_;
        dlc_                = other.dlc_;
        memcpy(data_, other.data_, dlc_.toLength());
        return (*this);
    }

    /// Equality Operator
    bool operator==(const Frame& other) const noexcept
    {
        return (id_ == other.id_) and (dlc_ == other.dlc_) and (::memcmp(data_, other.data_, dlc_.toLength()) == 0);
    }

    /// Inequality Operator
    bool operator!=(const Frame& other) const noexcept
    {
        return not operator==(other);
    }

    /// Used to determine at runtime if the frame has been correctly created
    /// @note A standard frame with an extended DLC would be considered invalid
    bool isValid() const noexcept
    {
        // if ID is extended the DLC doesn't matter
        // if the ID is not extended, the DLC can not be extended
        return (id_.isExtended() or (not dlc_.isExtended()));
    }

    /// Used to determine if a frame is Extended or Standard
    /// @note This can only be determined at runtime!
    bool isExtended() const noexcept
    {
        return id_.isExtended();
    }

    /// Used to fill the data buffer based on some functor.
    void forEach(std::function<std::uint8_t(std::size_t index)> functor) noexcept
    {
        for (std::size_t i = 0; i < dlc_.toLength(); i++)
        {
            data_[i] = functor(i);
        }
    }

    // #######################
    //  PUBLIC DATA MEMBERS
    // #######################

    /// Timestamp from when the frame was received at the CAN peripheral
    /// Ignored for TX frames (should be zero)
    time::Monotonic::MicrosecondType received_timestamp_;

    /// Contains the bits from the ID field in the raw frame
    ID_TYPE id_;

    /// Indicates how many frames after this frame caused an RX FIFO overflow and were lost.
    /// At a messaging rate of 1500 msg/sec with 16 bits, this could account for ~43 seconds worth of lost messages.
    /// @note This value will saturate at the type's max value.
    /// @warning Due to limitations in some drivers there may be one or more additional messages that were received
    /// after this one and before the messages were lost. The labeling of this as the last frame before dropped messages
    /// is a best effort.
    uint16_t frames_lost_;

    /// Flags if the received_timestamp value is valid. Ignored for tx frames.
    /// True if the driver was able to provide a valid rx timestamp.
    bool received_timestamp_valid_;

    /// The Data Length Code
    DataLengthCode dlc_;

    /// data portion of the frame
    uint8_t data_[MaxDataPayload];
};

/// The CAN 2.0b Extended Namespace
namespace extended
{
/// An Extended frame uses extended Identifiers and is sized for extended frames
using Frame = ::libcyphal::media::can::Frame<extended::Identifier>;
}  // namespace extended

/// The CAN 2.0b Standard Namespace
namespace standard
{
/// An Standard frame uses standard Identifiers and is sized for standard frames
using Frame = ::libcyphal::media::can::Frame<standard::Identifier>;
}  // namespace standard

/// A raw frame must be maximally sized. in order to draw a distinction between Extended and Standard
/// this type is called Raw (it can contain either).
using RawFrame = Frame<RawIdentifier>;

// TEMPLATE SPECIALIZATIONS. These allow copy construction of some types while deleting others.
/// @todo remove or limit use of templates
template <>  // Base Class
template <>  // Other Class
Frame<extended::Identifier>::Frame(const standard::Frame& other) = delete;

/// @todo remove or limit use of templates
template <>  // Base Class
template <>  // Other Class
Frame<standard::Identifier>::Frame(const extended::Frame& other) = delete;

}  // namespace can
}  // namespace media
}  // namespace libcyphal

#endif  // LIBCYPHAL_MEDIA_CAN_FRAME_HPP_INCLUDED
