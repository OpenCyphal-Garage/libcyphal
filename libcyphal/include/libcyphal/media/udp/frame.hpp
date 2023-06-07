/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// The UDP Frame format
/// @todo Move the 'media' folder under transport, update namespacing, etc.

#ifndef LIBCYPHAL_TRANSPORT_MEDIA_UDP_FRAME_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_MEDIA_UDP_FRAME_HPP_INCLUDED

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <udpard.h>
#include "libcyphal/introspection.hpp"
#include "libcyphal/types/time.hpp"

namespace libcyphal
{
/// @todo place under "transport" and remove media namespace
namespace media
{
namespace udp
{

/// The number of bytes in the transfer CRC.
/// @todo This is not currently used anywhere. Figure out if it is needed and remove otherwise.
constexpr static std::size_t TransferCrcSizeBytes = 4;

/// The maximum number of bytes this frame can hold. This value also effects the largest DLC the
/// instance will report and the largest DLC the instance will accept.
/// @todo Determine MTU and proper value for the MTU
constexpr static std::uint16_t MaximumMTUBytes = UDPARD_MTU_MAX;

/// @brief A raw UDP frame, as passed to/from a UDP peripheral or subsystem.
/// This is the datastructure used by the media layer of libcyphal to buffer incoming data.
/// @tparam  MTUBytesParam The maximum number of bytes that can be stored in this frame.
/// @todo Update redundancy behavior for UDP.
/// @todo This file is an awful lot like a UdpardFrame. Determine if this is needed or if we can just use UdpardFrame
class Frame final
{
public:
    /// @todo Use consistent naming for data_, vs payload etc., and also data_length_ vs payload_size, etc.
    std::uint8_t       data_[MaximumMTUBytes];
    std::uint_fast16_t data_length_;
    UdpardFrameHeader  header_;

    /// A monotonic timestamp. Libcyphal operates optimally when this value is a hardware supplied timestamp
    /// recorded at the start-of-frame.
    /// @todo This is currently recorded but nothing is done with the timestamp. Determine if it is needed and create
    ///   a JIRA to use it or remove it
    time::Monotonic timestamp_;

    /// @brief Generic constructor for Frame
    Frame() noexcept
        : data_{}
        , data_length_{0}
        , header_{}
        , timestamp_{}
    {
    }

    ~Frame() noexcept = default;

    /// @brief Copy constructor for frames.
    /// @param[in] rhs  The frame to copy from.
    Frame(const Frame& rhs) noexcept
        : data_{}
        , data_length_(rhs.data_length_)
        , header_{rhs.header_}
        , timestamp_(rhs.timestamp_)
    {
        if (this != &rhs)
        {
            std::copy_n(rhs.data_, MaximumMTUBytes, data_);
        }
    }

    /// @brief Move constructor for frames.
    /// @param[in] rhs  The frame to move from.
    /// @todo update no-throw swap functions isntead of std::copy_n
    Frame(Frame&& rhs) noexcept
    {
        if (this != &rhs)
        {
            data_length_ = rhs.data_length_;
            header_      = rhs.header_;
            timestamp_   = rhs.timestamp_;
            std::copy_n(rhs.data_, MaximumMTUBytes, data_);
            std::fill(std::begin(rhs.data_), std::end(rhs.data_), 0);
        }
    }

    /// @brief Constructs a new Frame object with timestamp that copies data into this instance
    /// @param[in] udp_data      The data to copy into this instance.
    /// @param[in] udp_data_size The data length length for the udp_data.
    /// @param[in] udp_header    UDP Header
    /// @param[in] udp_timestamp A monotonic timestamp that should be as close to the time the start-of-frame was
    Frame(volatile const std::uint8_t* udp_data,
          std::uint_fast16_t           udp_data_size,
          UdpardFrameHeader            udp_header,
          time::Monotonic              udp_timestamp) noexcept
        : data_{}
        , data_length_{udp_data_size}
        , header_{udp_header}
        , timestamp_{udp_timestamp}
    {
        std::copy_n(udp_data, (udp_data_size < MaximumMTUBytes) ? udp_data_size : MaximumMTUBytes, data_);
    }

    /// @brief Constructs a new Frame object with timestamp that copies data into this instance
    /// @param[in] udp_data      The data to copy into this instance.
    /// @param[in] udp_data_size The data length length for the udp_data.
    /// @param[in] udp_timestamp A monotonic timestamp that should be as close to the time the start-of-frame was
    Frame(volatile const std::uint8_t* udp_data,
          std::uint_fast16_t           udp_data_size,
          time::Monotonic              udp_timestamp) noexcept
        : data_{}
        , data_length_(udp_data_size)
        , header_{}
        , timestamp_(udp_timestamp)
    {
        std::copy_n(udp_data, (udp_data_size < MaximumMTUBytes) ? udp_data_size : MaximumMTUBytes, data_);
    }

    /// @brief Constructs a new Frame object that copies data into this instance.
    /// @param[in] udp_data      The data to copy into this instance.
    /// @param[in] udp_data_size The data length for the udp_data.
    Frame(volatile const std::uint8_t* udp_data, std::uint_fast16_t udp_data_size) noexcept
        : Frame(udp_data, udp_data_size, time::Monotonic::fromMicrosecond(0))
    {
    }

    /// @brief Copy assignment operator. This will copy all the data from rhs into this instance.
    /// @param[in] rhs   The frame to copy data from.
    Frame& operator=(const Frame& rhs) noexcept
    {
        if (this != &rhs)
        {
            timestamp_   = rhs.timestamp_;
            data_length_ = rhs.data_length_;
            header_      = rhs.header_;
            std::copy_n(rhs.data_, MaximumMTUBytes, data_);
        }
        return *this;
    }

    /// @brief Move assignment operator. This will copy all the data from rhs into this instance.
    /// @param[in] rhs   The frame to move data from.
    Frame& operator=(Frame&& rhs) noexcept
    {
        if (this != &rhs)
        {
            timestamp_   = rhs.timestamp_;
            data_length_ = rhs.data_length_;
            header_      = rhs.header_;
            std::copy_n(rhs.data_, MaximumMTUBytes, data_);
            std::fill(std::begin(rhs.data_), std::end(rhs.data_), 0);
        }
        return *this;
    }
};

}  // end namespace udp
}  // end namespace media
}  // end namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_MEDIA_UDP_FRAME_HPP_INCLUDED
