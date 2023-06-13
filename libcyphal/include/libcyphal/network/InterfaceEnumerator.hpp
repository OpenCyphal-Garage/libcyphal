/// @file
/// Abstract type for objects that enumerate available networking resources on a system.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef LIBCYPHAL_NETWORK_INTERFACE_ENUMERATOR_HPP_INCLUDED
#define LIBCYPHAL_NETWORK_INTERFACE_ENUMERATOR_HPP_INCLUDED

#include <cstddef>

namespace libcyphal
{
namespace network
{

template <typename IdType>
class InterfaceEnumerator
{
public:
    using pointer        = IdType*;
    using iterator       = pointer;
    using const_iterator = const IdType*;
    using size_type      = std::size_t;

    virtual const_iterator begin() const noexcept = 0;
    virtual const_iterator end() const noexcept   = 0;
    virtual size_type      count() const noexcept = 0;

    /// TODO: support interface hot-plugging. Yes, USB is a thing. Sorry.
    // e.g. virtual registerHotplugCallback(std::function<void(IdType)> callback) noexcept = 0;

protected:
    virtual ~InterfaceEnumerator() = default;
};

}  // namespace network
}  // namespace libcyphal

#endif  // LIBCYPHAL_NETWORK_INTERFACE_ENUMERATOR_HPP_INCLUDED
