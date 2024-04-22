/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_MULTIPLEXER_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_MULTIPLEXER_HPP_INCLUDED

namespace libcyphal
{
namespace transport
{

class IMultiplexer
{
public:
    IMultiplexer(const IMultiplexer&)                = delete;
    IMultiplexer(IMultiplexer&&) noexcept            = delete;
    IMultiplexer& operator=(const IMultiplexer&)     = delete;
    IMultiplexer& operator=(IMultiplexer&&) noexcept = delete;
    virtual ~IMultiplexer()                          = default;

    // TODO: Add methods here

protected:
    IMultiplexer() = default;

};  // IMultiplexer

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_MULTIPLEXER_HPP_INCLUDED
