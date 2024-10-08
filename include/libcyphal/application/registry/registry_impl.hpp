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

    cetl::optional<SetError> set(const IRegister::Name name, const Value& new_value) override
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
        return IRegister::Name{};
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

        CETL_DEBUG_ASSERT(reg.isLinked(), "Should be linked now.");
        CETL_DEBUG_ASSERT(std::get<0>(register_existing) != nullptr, "");
        return !std::get<1>(register_existing);
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
