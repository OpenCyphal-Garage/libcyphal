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
    virtual cetl::optional<IRegister::ValueAndFlags> get(const Name name) const = 0;

    /// Assigns the register with the specified value.
    ///
    /// The worst-case complexity is log(n), where n is the number of registers.
    ///
    /// @return Empty if value was set successfully, otherwise the error.
    ///
    virtual cetl::optional<SetError> set(const Name name, const Value& new_value) = 0;

protected:
    IRegistry()  = default;
    ~IRegistry() = default;

};  // IRegistry

// MARK: -

/// Extends the basic registry interface with additional methods that enable introspection.
///
class IIntrospectableRegistry : public IRegistry
{
public:
    IIntrospectableRegistry(IIntrospectableRegistry&&)                 = delete;
    IIntrospectableRegistry(const IIntrospectableRegistry&)            = delete;
    IIntrospectableRegistry& operator=(IIntrospectableRegistry&&)      = delete;
    IIntrospectableRegistry& operator=(const IIntrospectableRegistry&) = delete;

    /// Gets the total number of registers in the registry.
    ///
    /// The worst-case complexity may be linear in the number of registers.
    ///
    virtual std::size_t size() const = 0;

    /// Gets the name of the register at the specified index.
    ///
    /// The ordering is arbitrary but stable as long as the register set is not modified.
    /// The worst-case complexity may be linear in the number of registers.
    ///
    /// @param index The index of the register.
    /// @return Name of the register. `nullptr` if the index is out of range.
    ///
    virtual Name index(const std::size_t index) const = 0;

    /// Appends a new register to the registry.
    ///
    /// The worst-case complexity may be linear in the number of registers.
    ///
    /// @param reg The register to append. Should not be yet linked to a registry.
    /// @return True if the register was appended successfully.
    ///         False if the register with the same name already exists.
    ///
    virtual bool append(IRegister& reg) = 0;

protected:
    IIntrospectableRegistry()  = default;
    ~IIntrospectableRegistry() = default;

};  // IIntrospectableRegistry

}  // namespace registry
}  // namespace application
}  // namespace libcyphal

#endif  // LIBCYPHAL_APPLICATION_REGISTRY_HPP_INCLUDED
