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

/// @brief Defines interface to a custom multiplexer implementation.
///
/// Implementation is supposed to be provided by an user of the library.
///
class IMultiplexer
{
public:
    IMultiplexer(const IMultiplexer&)                = delete;
    IMultiplexer(IMultiplexer&&) noexcept            = delete;
    IMultiplexer& operator=(const IMultiplexer&)     = delete;
    IMultiplexer& operator=(IMultiplexer&&) noexcept = delete;

    // TODO: Add methods here

protected:
    IMultiplexer()  = default;
    ~IMultiplexer() = default;

};  // IMultiplexer

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_MULTIPLEXER_HPP_INCLUDED
