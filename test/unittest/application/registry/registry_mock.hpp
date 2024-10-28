/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_APPLICATION_REGISTRY_MOCK_HPP_INCLUDED
#define LIBCYPHAL_APPLICATION_REGISTRY_MOCK_HPP_INCLUDED

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/application/registry/register.hpp>
#include <libcyphal/application/registry/registry.hpp>

#include <gmock/gmock.h>

namespace libcyphal
{
namespace application
{
namespace registry
{

class RegistryMock : public IRegistry
{
public:
    RegistryMock()          = default;
    virtual ~RegistryMock() = default;

    RegistryMock(const RegistryMock&)                = delete;
    RegistryMock(RegistryMock&&) noexcept            = delete;
    RegistryMock& operator=(const RegistryMock&)     = delete;
    RegistryMock& operator=(RegistryMock&&) noexcept = delete;

    // MARK: IRegistry

    MOCK_METHOD(cetl::optional<IRegister::ValueAndFlags>, get, (const IRegister::Name name), (const, override));
    MOCK_METHOD(cetl::optional<SetError>,
                set,
                (const IRegister::Name name, const IRegister::Value& new_value),
                (override));

};  // RegistryMock

// MARK: -

class IntrospectableRegistryMock : public IIntrospectableRegistry
{
public:
    IntrospectableRegistryMock()          = default;
    virtual ~IntrospectableRegistryMock() = default;

    IntrospectableRegistryMock(const IntrospectableRegistryMock&)                = delete;
    IntrospectableRegistryMock(IntrospectableRegistryMock&&) noexcept            = delete;
    IntrospectableRegistryMock& operator=(const IntrospectableRegistryMock&)     = delete;
    IntrospectableRegistryMock& operator=(IntrospectableRegistryMock&&) noexcept = delete;

    // MARK: IRegistry

    MOCK_METHOD(cetl::optional<IRegister::ValueAndFlags>, get, (const IRegister::Name name), (const, override));
    MOCK_METHOD(cetl::optional<SetError>,
                set,
                (const IRegister::Name name, const IRegister::Value& new_value),
                (override));

    // MARK: IIntrospectableRegistry

    MOCK_METHOD(std::size_t, size, (), (const, override));
    MOCK_METHOD(IRegister::Name, index, (const std::size_t index), (const, override));
    MOCK_METHOD(bool, append, (IRegister & reg), (override));

};  // IntrospectableRegistryMock

}  // namespace registry
}  // namespace application
}  // namespace libcyphal

#endif  // LIBCYPHAL_APPLICATION_REGISTRY_MOCK_HPP_INCLUDED
