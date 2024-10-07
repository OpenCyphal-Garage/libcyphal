/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_APPLICATION_REGISTRY_IMPL_HPP_INCLUDED
#define LIBCYPHAL_APPLICATION_REGISTRY_IMPL_HPP_INCLUDED

#include "libcyphal/common/cavl/cavl.hpp"
#include "registry.hpp"

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
class Registry final : public IRegistry
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

    /// MARK: - IRegistry

    cetl::optional<IRegister::ValueAndFlags> get(const IRegister::Name name) const override
    {
        if (const auto* const reg = findRegisterBy(name))
        {
            return reg->get();
        }
        return cetl::nullopt;
    }

    cetl::optional<SetError> set(const IRegister::Name name, const Value& new_value) override
    {
        if (auto* const reg = findRegisterBy(name))
        {
            return reg->set(new_value);
        }
        return SetError::Existence;
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

}  // namespace registry
}  // namespace application
}  // namespace libcyphal

#endif  // LIBCYPHAL_APPLICATION_REGISTRY_IMPL_HPP_INCLUDED
