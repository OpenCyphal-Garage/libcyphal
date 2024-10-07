/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_APPLICATION_REGISTRY_REGISTER_IMPL_HPP_INCLUDED
#define LIBCYPHAL_APPLICATION_REGISTRY_REGISTER_IMPL_HPP_INCLUDED

#include "registry_impl.hpp"
#include "registry_value.hpp"

#include <utility>

namespace libcyphal
{
namespace application
{
namespace registry
{

class RegisterBase : public IRegister
{
    using Base = IRegister;

public:
    /// Options used when creating a new register.
    struct Options final
    {
        /// True if the register value is retained across application restarts.
        bool persistent{false};

    };  // Options

    RegisterBase(const RegisterBase&)                      = delete;
    RegisterBase& operator=(const RegisterBase&)           = delete;
    RegisterBase& operator=(RegisterBase&& other) noexcept = delete;

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
    ValueAndFlags getImpl(const T& value, bool is_mutable) const
    {
        ValueAndFlags out{Value{allocator_}, {}};
        registry::set(out.value_, value);
        out.flags_.mutable_    = is_mutable;
        out.flags_.persistent_ = options_.persistent;
        return out;
    }

    template <typename T, typename Setter>
    cetl::optional<SetError> setImpl(const T& value, Setter&& setter)
    {
        auto converted = get().value_;
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

template <typename Getter, typename Setter, bool IsMutable>
class Register;
//
template <typename Getter>
class Register<Getter, void, false> final : public RegisterBase
{
    using Base = RegisterBase;

public:
    virtual ~Register()  = default;
    Register(Register&&) = default;

    Register(const Register&)            = delete;
    Register& operator=(const Register&) = delete;
    Register& operator=(Register&&)      = delete;

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
    friend class Registry;

    explicit Register(cetl::pmr::memory_resource& memory, const Name name, const Options& options, Getter getter)
        : Base{memory, name, options}
        , getter_(std::move<Getter>(getter))
    {
    }

    // MARK: Data members:

    Getter getter_;

};  // Register<IsMutable=false>
//
template <typename Getter, typename Setter>
class Register<Getter, Setter, true> final : public RegisterBase
{
    using Base = RegisterBase;

public:
    virtual ~Register()  = default;
    Register(Register&&) = default;

    Register(const Register&)            = delete;
    Register& operator=(const Register&) = delete;
    Register& operator=(Register&&)      = delete;

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
    friend class Registry;

    explicit Register(cetl::pmr::memory_resource& memory,
                      const Name                  name,
                      const Options&              options,
                      Getter                      getter,
                      Setter                      setter)
        : Base{memory, name, options}
        , getter_{std::move<Getter>(getter)}
        , setter_{std::move<Getter>(setter)}
    {
    }

    // MARK: Data members:

    Getter getter_;
    Setter setter_;

};  // Register<IsMutable=true>

// MARK: -

template <typename ValueType, bool IsMutable = true>
class ParamRegister final : public RegisterBase
{
public:
    ParamRegister(Registry& rgy, const Name name, const ValueType& default_value, const Options& options = {true})
        : RegisterBase{rgy.memory(), name, options}
        , value_{default_value}
    {
        (void) rgy;
    }

    virtual ~ParamRegister()       = default;
    ParamRegister(ParamRegister&&) = default;

    ParamRegister(const ParamRegister&)            = delete;
    ParamRegister& operator=(const ParamRegister&) = delete;
    ParamRegister& operator=(ParamRegister&&)      = delete;

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
            value_ = registry::get<ValueType>(value).value();
            return true;
        });
    }

private:
    friend class Registry;

    // MARK: Data members:

    ValueType value_;

};  // ParamRegister

}  // namespace registry
}  // namespace application
}  // namespace libcyphal

#endif  // LIBCYPHAL_APPLICATION_REGISTRY_REGISTER_IMPL_HPP_INCLUDED
