/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_APPLICATION_REGISTRY_IMPL_HPP_INCLUDED
#define LIBCYPHAL_APPLICATION_REGISTRY_IMPL_HPP_INCLUDED

#include "libcyphal/common/cavl/cavl.hpp"
#include "register.hpp"
#include "register_impl.hpp"
#include "registry.hpp"
#include "registry_value.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>

namespace libcyphal
{
namespace application
{
namespace registry
{

/// Defines the registry implementation.
///
class Registry final : public IIntrospectableRegistry
{
public:
    /// Constructs a new registry.
    ///
    /// @param memory The memory resource to use for variable size-d register values.
    ///
    explicit Registry(cetl::pmr::memory_resource& memory)
        : memory_{memory}
    {
    }
    ~Registry() = default;

    Registry(Registry&&)                 = delete;
    Registry(const Registry&)            = delete;
    Registry& operator=(Registry&&)      = delete;
    Registry& operator=(const Registry&) = delete;

    cetl::pmr::memory_resource& memory() const noexcept
    {
        return memory_;
    }

    // MARK: - IRegistry

    cetl::optional<IRegister::ValueAndFlags> get(const Name name) const override
    {
        if (const auto* const reg = findRegisterBy(name))
        {
            return reg->get();
        }
        return cetl::nullopt;
    }

    cetl::optional<SetError> set(const Name name, const Value& new_value) override
    {
        if (auto* const reg = findRegisterBy(name))
        {
            return reg->set(new_value);
        }
        return SetError::Existence;
    }

    // MARK: - IIntrospectableRegistry

    std::size_t size() const override
    {
        return registers_tree_.size();
    }

    Name index(const std::size_t index) const override
    {
        if (const auto* const reg = registers_tree_[index])
        {
            return reg->getName();
        }
        return Name{};
    }

    bool append(IRegister& reg) override
    {
        CETL_DEBUG_ASSERT(!reg.isLinked(), "Should not be linked yet.");

        auto register_existing = registers_tree_.search(
            [key = reg.getKey()](const IRegister& other) {
                //
                return other.compareBy(key);
            },
            [&reg]() -> IRegister* {
                //
                return &reg;
            });

        CETL_DEBUG_ASSERT(std::get<0>(register_existing) != nullptr, "");
        CETL_DEBUG_ASSERT(std::get<0>(register_existing)->isLinked(), "Should be linked.");
        return !std::get<1>(register_existing);
    }

    // MARK: - Other factory methods:

    /// Constructs a new read-only register, and links it to this registry.
    ///
    /// @param name The name of the register. Should be unique within the registry.
    /// @param getter The getter function to provide the register value.
    /// @param options Extra options for the register, like "persistent" option.
    /// @return Result register if it was appended successfully. Otherwise, `nullopt`.
    ///
    template <typename Getter>
    cetl::optional<Register<Getter, void>> route(const Name                name,
                                                 Getter&&                  getter,
                                                 const IRegister::Options& options = {})
    {
        auto reg = makeRegister(memory(), name, std::forward<Getter>(getter), options);
        if (append(reg))
        {
            return reg;
        }
        return cetl::nullopt;
    }

    /// Constructs a new read-write register, and links it to this registry.
    ///
    /// @param name The name of the register. Should be unique within the registry.
    /// @param getter The getter function to provide the register value.
    /// @param setter The setter function to update the register value.
    /// @param options Extra options for the register, like "persistent" option.
    /// @return Result register if it was appended successfully. Otherwise, `nullopt`.
    ///
    template <typename Getter, typename Setter>
    cetl::optional<Register<Getter, Setter>> route(const Name                name,
                                                   Getter&&                  getter,
                                                   Setter&&                  setter,
                                                   const IRegister::Options& options = {})
    {
        auto reg = makeRegister(memory(), name, std::forward<Getter>(getter), std::forward<Setter>(setter), options);
        if (append(reg))
        {
            return reg;
        }
        return cetl::nullopt;
    }

    /// Constructs a read-write register, and links it to a given registry.
    ///
    /// A simple wrapper over route() that allows one to expose and mutate an arbitrary object as a mutable register.
    ///
    /// @param name The name of the register. Should be unique within the registry.
    /// @param inout_value The referenced value; shall outlive the register.
    /// @param options Extra options for the register, like "persistent" option.
    /// @return Result register if it was appended successfully. Otherwise, `nullopt`.
    ///
    template <typename T>
    auto expose(const Name name, T& inout_value, const IRegister::Options& options = {})
    {
        return route(
            name,
            [&inout_value]() -> const T& { return inout_value; },  // Getter
            [&inout_value](const Value& v) {                       // Setter
                //
                inout_value = registry::get<T>(v).value();  // Guaranteed to be coercible by the protocol.
                return true;
            },
            options);
    }

    /// Constructs a parameter register, and links it to a given registry.
    ///
    /// In contrast to the above `expose()`, this method allows one to expose a parameter with a default value.
    /// Exposed parameter value is stored inside the register and can be mutated (by default).
    ///
    /// @param name The name of the register. Should be unique within the registry.
    /// @param default_value Initial default value.
    /// @param options Extra options for the register, like "persistent" option.
    /// @return Result register if it was appended successfully. Otherwise, `nullopt`.
    ///
    template <typename T, bool IsMutable = true>
    cetl::optional<ParamRegister<T, IsMutable>> exposeParam(const Name                name,
                                                            const T&                  default_value,
                                                            const IRegister::Options& options = {})
    {
        ParamRegister<T, IsMutable> reg{memory(), name, default_value, options};
        if (append(reg))
        {
            return reg;
        }
        return cetl::nullopt;
    }

private:
    CETL_NODISCARD IRegister* findRegisterBy(const Name name)
    {
        return registers_tree_.search(
            [key = IRegister::Key{name}](const IRegister& other) { return other.compareBy(key); });
    }

    CETL_NODISCARD const IRegister* findRegisterBy(const Name name) const
    {
        return registers_tree_.search(
            [key = IRegister::Key{name}](const IRegister& other) { return other.compareBy(key); });
    }

    cetl::pmr::memory_resource& memory_;
    cavl::Tree<IRegister>       registers_tree_;

};  // Registry

}  // namespace registry
}  // namespace application
}  // namespace libcyphal

#endif  // LIBCYPHAL_APPLICATION_REGISTRY_IMPL_HPP_INCLUDED
