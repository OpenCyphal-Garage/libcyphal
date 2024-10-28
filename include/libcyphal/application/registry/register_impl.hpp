/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_APPLICATION_REGISTRY_REGISTER_IMPL_HPP_INCLUDED
#define LIBCYPHAL_APPLICATION_REGISTRY_REGISTER_IMPL_HPP_INCLUDED

#include "register.hpp"

#include <cetl/cetl.hpp>

#include <utility>

namespace libcyphal
{
namespace application
{
namespace registry
{
/// Defines abstract base class for a register implementation.
///
/// Implements common functionality for all register types like name, options, and value accessors.
///
class RegisterBase : public IRegister
{
    using Base = IRegister;

public:
    RegisterBase(const RegisterBase&)                      = delete;
    RegisterBase& operator=(const RegisterBase&)           = delete;
    RegisterBase& operator=(RegisterBase&& other) noexcept = delete;

    // MARK: IRegister

    Name getName() const override
    {
        return name_;
    }

protected:
    RegisterBase(cetl::pmr::memory_resource& memory, const Name name, const Options& options)
        : Base{name}
        , name_{name}
        , options_{options}
        , allocator_{&memory}
    {
    }

    ~RegisterBase()                       = default;
    RegisterBase(RegisterBase&&) noexcept = default;

    ValueAndFlags getImpl(const Value& value, const bool is_mutable) const
    {
        return {value, {is_mutable, options_.persistent}};
    }

    template <typename T>
    ValueAndFlags getImpl(const T& value, const bool is_mutable) const
    {
        ValueAndFlags out{Value{allocator_}, {is_mutable, options_.persistent}};
        out.value.union_value.emplace<T>(value, allocator_);
        return out;
    }

private:
    // MARK: Data members:

    const Name            name_;
    const Options         options_;
    Value::allocator_type allocator_;

};  // RegisterBase

// MARK: -

/// Defines a read-write register implementation.
///
/// @tparam Getter The getter function `T()` type, where `T` is either `Value` or one of its variants.
/// @tparam Setter The setter function `cetl::optional<SetError>(const Value&)` type.
///
/// The actual value is provided by the getter function,
/// and the setter function is used to update the value.
///
template <typename Getter, typename Setter>
class RegisterImpl final : public RegisterBase
{
    using Base = RegisterBase;

public:
    /// Constructs a new read-write detached register, which is not yet linked to any registry (aka detached).
    ///
    /// A detached register must be appended to a registry before its value could be exposed by the registry.
    /// Use `makeRegister<Getter, Setter>()` factory function to automatically deduce the template types.
    /// Alternatively, use the following registry methods to create and link the register in one step:
    /// - `registry.route<Getter, Setter>(name, getter, setter, options)`
    ///
    /// @param memory The memory resource to use for variable size-d register values.
    /// @param name The name of the register.
    /// @param getter The getter function to provide the register value.
    /// @param setter The setter function to update the register value.
    /// @param options Extra options for the register, like "persistent" option.
    ///
    RegisterImpl(cetl::pmr::memory_resource& memory,
                 const Name                  name,
                 Getter&&                    getter,
                 Setter&&                    setter,
                 const Options&              options = {})
        : Base{memory, name, options}
        , getter_{std::move(getter)}
        , setter_{std::move(setter)}
    {
    }

    ~RegisterImpl()                       = default;
    RegisterImpl(RegisterImpl&&) noexcept = default;

    RegisterImpl(const RegisterImpl&)                = delete;
    RegisterImpl& operator=(const RegisterImpl&)     = delete;
    RegisterImpl& operator=(RegisterImpl&&) noexcept = delete;

    // MARK: IRegister

    ValueAndFlags get() const override
    {
        return getImpl(getter_(), true);
    }

    cetl::optional<SetError> set(const Value& new_value) override
    {
        return setter_(new_value);
    }

private:
    // MARK: Data members:

    Getter getter_;
    Setter setter_;
};
/// Defines a read-only register implementation.
///
/// @tparam Getter The getter function `T()` type, where `T` is either `Value` or one of its variants.
///
/// The actual value is provided by the getter function.
///
template <typename Getter>
class RegisterImpl<Getter, void> final : public RegisterBase
{
    using Base = RegisterBase;

public:
    /// Constructs a new read-only register, which is not yet linked to any registry (aka detached).
    ///
    /// A detached register must be appended to a registry before its value could be exposed by the registry.
    /// Use `makeRegister<Getter>()` factory function to automatically deduce the template types.
    /// Alternatively, use `registry.route<Getter>(name, options, getter)` method
    /// to create and link the register in one step.
    ///
    /// @param memory The memory resource to use for variable size-d register values.
    /// @param name The name of the register.
    /// @param getter The getter function to provide the register value.
    /// @param options Extra options for the register, like "persistent" option.
    ///
    RegisterImpl(cetl::pmr::memory_resource& memory, const Name name, Getter&& getter, const Options& options = {})
        : Base{memory, name, options}
        , getter_(std::move(getter))
    {
    }

    ~RegisterImpl()                       = default;
    RegisterImpl(RegisterImpl&&) noexcept = default;

    RegisterImpl(const RegisterImpl&)                = delete;
    RegisterImpl& operator=(const RegisterImpl&)     = delete;
    RegisterImpl& operator=(RegisterImpl&&) noexcept = delete;

    // MARK: IRegister

    ValueAndFlags get() const override
    {
        return getImpl(getter_(), false);
    }

    cetl::optional<SetError> set(const Value&) override
    {
        return SetError::Mutability;
    }

private:
    // MARK: Data members:

    Getter getter_;

};  // RegisterImpl

/// Constructs a new read-only register, which is not yet linked to any registry (aka detached).
///
/// A detached register must be appended to a registry before its value could be exposed by the registry.
/// Alternatively, use `registry.route<Getter>(name, getter, options)` method
/// to create and link the register in one step.
///
/// @param memory The memory resource to use for variable size-d register values.
///               Note that the memory resource is not used for creation of the register itself (it's done on stack
///               and returned by move) but for the transient variable size-d values of the register.
/// @param name The name of the register.
/// @param getter The getter function to provide the register value.
/// @param options Extra options for the register, like "persistent" option.
/// @return The constructed detached register.
///
template <typename Getter>
RegisterImpl<Getter, void> makeRegister(cetl::pmr::memory_resource& memory,
                                        const IRegister::Name       name,
                                        Getter&&                    getter,
                                        const IRegister::Options&   options = {})
{
    return RegisterImpl<Getter, void>{memory, name, std::forward<Getter>(getter), options};
}

/// Constructs a new read-write register, which is not yet linked to any registry (aka detached).
///
/// A detached register must be appended to a registry before its value could be exposed by the registry.
/// Alternatively, use the following registry methods to create and link the register in one step:
/// - `registry.route<Getter, Setter>(name, getter, setter, options)`
/// - `registry.expose<T>(name, T& value, options)`
///
/// @param memory The memory resource to use for variable size-d register values.
///               Note that the memory resource is not used for creation of the register itself (it's done on stack
///               and returned by move) but for the transient variable size-d values of the register.
/// @param name The name of the register.
/// @param getter The getter function to provide the register value.
/// @param setter The setter function to update the register value.
/// @param options Extra options for the register, like "persistent" option.
/// @return The constructed detached register.
///
template <typename Getter, typename Setter>
RegisterImpl<Getter, Setter> makeRegister(cetl::pmr::memory_resource& memory,
                                          const IRegister::Name       name,
                                          Getter&&                    getter,
                                          Setter&&                    setter,
                                          const IRegister::Options&   options = {})
{
    return RegisterImpl<Getter, Setter>{memory,
                                        name,
                                        std::forward<Getter>(getter),
                                        std::forward<Setter>(setter),
                                        options};
}

}  // namespace registry
}  // namespace application
}  // namespace libcyphal

#endif  // LIBCYPHAL_APPLICATION_REGISTRY_REGISTER_IMPL_HPP_INCLUDED
