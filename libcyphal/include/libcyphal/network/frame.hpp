/// @file
/// Single IP data frame type.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef LIBCYPHAL_NETWORK_IP_FRAME_HPP_INCLUDED
#define LIBCYPHAL_NETWORK_IP_FRAME_HPP_INCLUDED

#include "libcyphal/network/ip/address.hpp"
#include "cetl/pf20/span.hpp"
#include "cetl/pf17/byte.hpp"

namespace libcyphal
{
namespace network
{

template<std::size_t MTU>
class Frame
{
public:
    static constexpr std::size_t Mtu = MTU;
    using byte_type = cetl::pf17::byte;
    using size_type = std::size_t;

    template<typename T>
    using span_type = cetl::pf20::span<T>;

    span_type<const byte_type> constView() const noexcept
    {
        return span_type<const byte_type>{data_, fill_};
    }

    span_type<const byte_type> mutableView() noexcept
    {
        return span_type<byte_type>{data_, fill_};
    }

    size_type length() const noexcept
    {
        return fill_;
    }

    size_type size_bytes() const noexcept
    {
        return sizeof(byte_type) * fill_;
    }
private:
    byte_type data_[MTU];
    size_type fill_{0};
};

}  // namespace network
}  // namespace libcyphal

#endif  // LIBCYPHAL_NETWORK_IP_FRAME_HPP_INCLUDED
