/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_APPLICATION_REGISTRY_HPP_INCLUDED
#define LIBCYPHAL_APPLICATION_REGISTRY_HPP_INCLUDED

#include "register.hpp"

#include <cetl/pf17/cetlpf.hpp>

namespace libcyphal
{
namespace application
{
namespace registry
{

/// Defines interface for a registry.
///
class IRegistry
{
public:
    IRegistry(IRegistry&&)                 = delete;
    IRegistry(const IRegistry&)            = delete;
    IRegistry& operator=(IRegistry&&)      = delete;
    IRegistry& operator=(const IRegistry&) = delete;

    /// Reads the current value and flags of the register.
    ///
    /// The worst-case complexity is log(n), where n is the number of registers.
    ///
    /// @return Value and flags. Empty if nonexistent.
    ///
    virtual cetl::optional<IRegister::ValueAndFlags> get(const IRegister::Name name) const = 0;

    /// Assigns the register with the specified value.
    ///
    /// The worst-case complexity is log(n), where n is the number of registers.
    ///
    /// @return Empty if value was set successfully, otherwise the error.
    ///
    virtual cetl::optional<SetError> set(const IRegister::Name name, const Value& new_value) = 0;

protected:
    IRegistry()  = default;
    ~IRegistry() = default;

};  // IRegistry

}  // namespace registry
}  // namespace application
}  // namespace libcyphal

#endif  // LIBCYPHAL_APPLICATION_REGISTRY_HPP_INCLUDED
