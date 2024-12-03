/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_ERRORS_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_ERRORS_HPP_INCLUDED

#include "libcyphal/config.hpp"
#include "libcyphal/errors.hpp"
#include "libcyphal/types.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/rtti.hpp>
#include <cetl/unbounded_variant.hpp>

#include <cstdint>

namespace libcyphal
{
namespace transport
{

struct AnonymousError final
{};

struct CapacityError final
{};

/// @brief Defines platform-specific error.
///
class IPlatformError
{
    // C6271889-BCF8-43A9-8D79-FA64FC3EFD93
    // clang-format off
    using TypeIdType = cetl::type_id_type<
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
        0xC6, 0x27, 0x18, 0x89, 0xBC, 0xF8, 0x43, 0xA9, 0x8D, 0x79, 0xFA, 0x64, 0xFC, 0x3E, 0xFD, 0x93>;
    // clang-format on

public:
    /// Gets platform-specific error code.
    ///
    virtual std::uint32_t code() const noexcept = 0;

    // MARK: RTTI

    static constexpr cetl::type_id _get_type_id_() noexcept
    {
        return cetl::type_id_type_value<TypeIdType>();
    }

protected:
    IPlatformError()                                     = default;
    ~IPlatformError()                                    = default;
    IPlatformError(const IPlatformError&)                = default;
    IPlatformError(IPlatformError&&) noexcept            = default;
    IPlatformError& operator=(const IPlatformError&)     = default;
    IPlatformError& operator=(IPlatformError&&) noexcept = default;
};
using PlatformError =
    ImplementationCell<IPlatformError, cetl::unbounded_variant<config::Transport::PlatformErrorMaxSize()>>;

struct AlreadyExistsError final
{};

/// @brief Defines any possible error at Cyphal transport layer.
///
/// General taxonomy of results of transport layer methods is such that:
/// - A method returns (via `cetl::variant`) either an expected `Success` type, or a `Failure` type.
/// - If the success result type is `void`, then `cetl::optional<Failure>` in in use (instead of `cetl::variant`).
/// - The failure result type is a `cetl::variant` of all possible "primitive" error types that may occur in the method.
///   The "Failure" suffix is used to denote such variant types; "Error" suffix denotes the "primitive" error types.
/// - Some methods may have a limited set of expected error types (comparing f.e. with broader set of `AnyFailure`),
///   in which case a custom `cetl::variant` failure type is defined (see below `FactoryFailure` or `MediaFailure`).
/// - For convenience, some interface methods might have their own `<MethodName>Result` umbrella result type defined
///   within the interface. Such umbrella result types denoted with "Result" suffix, and typically contain their own
///   `Success`, `Failure` and `Type` nested types/aliases (see f.e. `ITxSocket` or `IRxSocket` interfaces).
///
using AnyFailure =  //
    cetl::variant<AnonymousError, ArgumentError, MemoryError, CapacityError, PlatformError, AlreadyExistsError>;

/// @brief Defines any possible factory error at Cyphal transport layer.
///
using FactoryFailure = cetl::variant<ArgumentError, MemoryError>;

/// @brief Defines any possible error at Cyphal media layer.
///
using MediaFailure = cetl::variant<ArgumentError, PlatformError, CapacityError>;

}  // namespace transport
}  // namespace libcyphal

namespace cetl
{

// C6271889-BCF8-43A9-8D79-FA64FC3EFD93
template <>
constexpr type_id type_id_getter<libcyphal::transport::AnyFailure>() noexcept
{
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
    return {0xC6, 0x27, 0x18, 0x89, 0xBC, 0xF8, 0x43, 0xA9, 0x8D, 0x79, 0xFA, 0x64, 0xFC, 0x3E, 0xFD, 0x93};
}

}  // namespace cetl

#endif  // LIBCYPHAL_TRANSPORT_ERRORS_HPP_INCLUDED
