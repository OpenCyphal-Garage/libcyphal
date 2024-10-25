/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_APPLICATION_REGISTRY_REGISTER_IMPL_HPP_INCLUDED
#define LIBCYPHAL_APPLICATION_REGISTRY_REGISTER_IMPL_HPP_INCLUDED

#include "registry_value.hpp"

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

    /// Gets create options of this register.
    ///
    Options getOptions() const noexcept
    {
        return options_;
    }

    // MARK: IRegister

    Name getName() const override
    {
        return name_;
    }

protected:
    RegisterBase(cetl::pmr::memory_resource& memory, const Name name, const Options& options)
        : Base{name}
        , allocator_{&memory}
        , name_{name}
        , options_{options}
    {
    }

    ~RegisterBase()                       = default;
    RegisterBase(RegisterBase&&) noexcept = default;

    template <typename T>
    ValueAndFlags getImpl(const T& value, const bool is_mutable) const
    {
        ValueAndFlags out{Value{allocator_}, {}};
        registry::set(out.value, value);
        out.flags._mutable   = is_mutable;
        out.flags.persistent = options_.persistent;
        return out;
    }

    template <typename T, typename Setter>
    cetl::optional<SetError> setImpl(const T& value, Setter&& setter)
    {
        auto converted = get().value;
        if (coerce(converted, value))
        {
            if (std::forward<Setter>(setter)(converted))
            {
                return cetl::nullopt;
            }
            return SetError::Semantics;
        }
        return SetError::Coercion;
    }

private:
    // MARK: Data members:

    Value::allocator_type allocator_;
    const Name            name_;
    const Options         options_;

};  // RegisterBase

// MARK: -

/// Defines a read-write register implementation.
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
    /// - `registry.expose<T>(name, T& value, options)`
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
        return setImpl(new_value, setter_);
    }

private:
    // MARK: Data members:

    Getter getter_;
    Setter setter_;
};
/// Defines a read-only register implementation.
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

// MARK: -

/// Defines a parameter-based register implementation template.
///
/// Instead of "external" getter/setter functions, the register uses member value storage.
///
template <typename ValueType, bool IsMutable = true>
class ParamRegister final : public RegisterBase
{
public:
    /// Constructs a new parameter register, which is not yet linked to any registry.
    ///
    /// A detached register must be appended to a registry before its value could be exposed by the registry.
    /// Alternatively, use `registry.exposeParam<T, IsMutable>(name, const T& default_value, options)` method
    /// to create and link the parameter register in one step.
    ///
    /// @param memory The memory resource to use for variable size-d register values.
    /// @param name The name of the register.
    /// @param default_value The initial default value of the register.
    /// @param options Extra options for the register, like "persistent" option.
    ///
    ParamRegister(cetl::pmr::memory_resource& memory,
                  const Name                  name,
                  const ValueType&            default_value,
                  const Options&              options = {})
        : RegisterBase{memory, name, options}
        , value_{default_value}
    {
    }

    ~ParamRegister()                        = default;
    ParamRegister(ParamRegister&&) noexcept = default;

    ParamRegister(const ParamRegister&)                = delete;
    ParamRegister& operator=(const ParamRegister&)     = delete;
    ParamRegister& operator=(ParamRegister&&) noexcept = delete;

    ValueType& value() noexcept
    {
        return value_;
    }

    // MARK: IRegister

    ValueAndFlags get() const override
    {
        return getImpl(value_, IsMutable);
    }

    cetl::optional<SetError> set(const Value& new_value) override
    {
        if (!IsMutable)
        {
            return SetError::Mutability;
        }

        return setImpl(new_value, [this](const Value& value) {
            //
            if (auto converted = registry::get<ValueType>(value))
            {
                value_ = converted.value();
                return true;
            }
            return false;
        });
    }

private:
    // MARK: Data members:

    ValueType value_;

};  // ParamRegister

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
                                        const Name                  name,
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
                                          const Name                  name,
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

/// Constructs a new read-write register, which is not yet linked to any registry (aka detached).
///
/// A detached register must be appended to a registry before its value could be exposed by the registry.
/// Alternatively, use the following registry method to create and link the register in one step:
/// - `registry.parameterize<T>(name, default_value, options)`
///
/// @tparam T The type of the parameter stored inside the register.
/// @tparam IsMutable Whether the register is mutable or not.
/// @tparam U The type of the default value - should be convertible to `T`.
///
/// @param memory The memory resource to use for variable size-d register values.
///               Note that the memory resource is not used for creation of the register itself (it's done on stack
///               and returned by move) but for the transient variable size-d values of the register.
/// @param name The name of the register.
/// @param default_value Initial default value.
/// @param options Extra options for the register, like "persistent" option.
/// @return The constructed detached register.
///
template <typename T, bool IsMutable = true, typename U = T>
auto makeParamRegister(cetl::pmr::memory_resource& memory,
                       const Name                  name,
                       const U&                    default_value,
                       const IRegister::Options&   options = {})
    -> std::enable_if_t<std::is_convertible<U, T>::value, ParamRegister<T, IsMutable>>
{
    return ParamRegister<T, IsMutable>{memory, name, default_value, options};
}

}  // namespace registry
}  // namespace application
}  // namespace libcyphal

#endif  // LIBCYPHAL_APPLICATION_REGISTRY_REGISTER_IMPL_HPP_INCLUDED
