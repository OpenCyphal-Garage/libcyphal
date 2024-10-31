/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_APPLICATION_REGISTRY_IMPL_HPP_INCLUDED
#define LIBCYPHAL_APPLICATION_REGISTRY_IMPL_HPP_INCLUDED

#include "libcyphal/common/cavl/cavl.hpp"
#include "libcyphal/platform/storage.hpp"
#include "register.hpp"
#include "register_impl.hpp"
#include "registry.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>

#include <uavcan/_register/Value_1_0.hpp>

#include <array>
#include <cstddef>

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

    cetl::optional<IRegister::ValueAndFlags> get(const IRegister::Name name) const override
    {
        if (const auto* const reg = findRegisterBy(name))
        {
            return reg->get();
        }
        return cetl::nullopt;
    }

    cetl::optional<SetError> set(const IRegister::Name name, const IRegister::Value& new_value) override
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

    IRegister::Name index(const std::size_t index) const override
    {
        if (const auto* const reg = registers_tree_[index])
        {
            return reg->getName();
        }
        return {};
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
    /// @return The result immutable register. Check its `.isLinked()` to verify it was appended successfully.
    ///
    template <typename Getter>
    RegisterImpl<Getter, void> route(const IRegister::Name     name,
                                     Getter&&                  getter,
                                     const IRegister::Options& options = {})
    {
        auto reg = makeRegister(memory(), name, std::forward<Getter>(getter), options);
        (void) append(reg);
        return reg;
    }

    /// Constructs a new read-write register, and links it to this registry.
    ///
    /// @param name The name of the register. Should be unique within the registry.
    /// @param getter The getter function to provide the register value.
    /// @param setter The setter function to update the register value.
    /// @param options Extra options for the register, like "persistent" option.
    /// @return The result mutable register. Check its `.isLinked()` to verify it was appended successfully.
    ///
    template <typename Getter, typename Setter>
    RegisterImpl<Getter, Setter> route(const IRegister::Name     name,
                                       Getter&&                  getter,
                                       Setter&&                  setter,
                                       const IRegister::Options& options = {})
    {
        auto reg = makeRegister(memory(), name, std::forward<Getter>(getter), std::forward<Setter>(setter), options);
        (void) append(reg);
        return reg;
    }

private:
    CETL_NODISCARD IRegister* findRegisterBy(const IRegister::Name name)
    {
        return registers_tree_.search(
            [key = IRegister::Key{name}](const IRegister& other) { return other.compareBy(key); });
    }

    CETL_NODISCARD const IRegister* findRegisterBy(const IRegister::Name name) const
    {
        return registers_tree_.search(
            [key = IRegister::Key{name}](const IRegister& other) { return other.compareBy(key); });
    }

    cetl::pmr::memory_resource& memory_;
    cavl::Tree<IRegister>       registers_tree_;

};  // Registry

// MARK: -

/// Saves all persistent mutable registers from the registry to the storage.
///
/// The register savior is the counterpart of load().
/// Registers that are not persistent OR not mutable will not be saved;
/// the reason immutable registers are not saved is that they are assumed to be constant or runtime-computed,
/// so there is no point wasting storage on them (which may be limited).
/// Eventually, this logic should be decoupled from the network register presentation facade by introducing more
/// fine-grained register flags, such as "internally mutable" and "externally mutable".
///
/// Existing stored registers that are not found in the registry will not be altered.
/// In case of failure, one failure handling strategy is to clear or reformat the entire storage and try again.
///
/// The removal predicate allows the caller to specify which registers need to be removed from the storage
/// instead of being saved. This is useful for implementing the "factory reset" feature.
///
/// @param kv The key-value storage to save the registers to.
/// @param rgy The registry to save the registers from.
/// @param reset_predicate The predicate to determine which registers should be removed from the storage.
///                        Should have `bool(const IRegister::Name)` signature.
/// @return Nothing in case of success.
///         Otherwise, the very first error encountered (on which we stopped the registry enumeration).
///
template <typename ResetPredicate>
auto save(platform::storage::IKeyValue&  kv,
          const IIntrospectableRegistry& rgy,
          const ResetPredicate&          reset_predicate) -> cetl::optional<platform::storage::Error>
{
    for (std::size_t index = 0; index < rgy.size(); index++)
    {
        const IRegister::Name reg_name = rgy.index(index);
        if (reg_name.empty())
        {
            // No more registers to store.
            break;
        }

        // Reset is handled before any other checks to enhance forward compatibility.
        if (reset_predicate(reg_name))
        {
            if (const auto err = kv.drop(reg_name))
            {
                if (err.value() != platform::storage::Error::Existence)
                {
                    return err;
                }
            }
            continue;
        }

        // If we get nothing, this means that the register has disappeared from the register.
        if (const auto reg_meta = rgy.get(reg_name))
        {
            // We do not save immutable registers because they are assumed to be constant, so no need to waste storage.
            if (reg_meta->flags.persistent && reg_meta->flags._mutable)
            {
                // We don't expect to have any serialization errors here,
                // b/c `SerializationBufferSizeBytes` sized buffer should always be big enough.
                std::array<std::uint8_t, IRegister::Value::_traits_::SerializationBufferSizeBytes> buffer;
                const auto buffer_size = serialize(reg_meta->value, buffer);
                CETL_DEBUG_ASSERT(buffer_size.has_value(), "");

                if (const auto err = kv.put(reg_name, {buffer.data(), buffer_size.value()}))
                {
                    return err;
                }
            }
        }

    }  // for every register

    return cetl::nullopt;
}
inline cetl::optional<platform::storage::Error> save(platform::storage::IKeyValue&  kv,
                                                     const IIntrospectableRegistry& rgy)
{
    return save(kv, rgy, [](const IRegister::Name) { return false; });
}

}  // namespace registry
}  // namespace application
}  // namespace libcyphal

#endif  // LIBCYPHAL_APPLICATION_REGISTRY_IMPL_HPP_INCLUDED
