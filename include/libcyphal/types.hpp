/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TYPES_HPP_INCLUDED
#define LIBCYPHAL_TYPES_HPP_INCLUDED

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pmr/interface_ptr.hpp>
#include <cetl/variable_length_array.hpp>

#include <chrono>
#include <cstdint>
#include <ratio>

namespace libcyphal
{

/// @brief The internal time representation is in microseconds.
///
/// This is in line with the lizards that use `uint64_t`-typed microsecond counters throughout.
///
struct MonotonicClock final
{
    using rep        = std::int64_t;
    using period     = std::micro;
    using duration   = std::chrono::duration<rep, period>;
    using time_point = std::chrono::time_point<MonotonicClock>;

    static constexpr bool is_steady = true;

    /// @brief Gets the current time point.
    ///
    /// Method is NOT implemented by the library; the user code is expected to provide a suitable implementation
    /// instead depending on the requirements of the application.
    /// A possible implementation on a POSIX-like platform is:
    /// ```
    /// MonotonicClock::time_point MonotonicClock::now() noexcept
    /// {
    ///     return std::chrono::time_point_cast<time_point>(std::chrono::steady_clock::now());
    /// }
    /// ```
    static time_point now() noexcept;

};  // MonotonicClock

using TimePoint = MonotonicClock::time_point;
using Duration  = MonotonicClock::duration;

template <typename T>
using UniquePtr = cetl::pmr::InterfacePtr<T>;

// TODO: Maybe introduce `cetl::expected` at CETL repo.
template <typename Success, typename Failure>
using Expected = cetl::variant<Success, Failure>;

namespace detail
{

template <typename T>
using PmrAllocator = cetl::pmr::polymorphic_allocator<T>;

template <typename T>
using VarArray = cetl::VariableLengthArray<T, PmrAllocator<T>>;

template <typename Spec, typename... Args>
CETL_NODISCARD UniquePtr<typename Spec::Interface> makeUniquePtr(cetl::pmr::memory_resource& memory, Args&&... args)
{
    return cetl::pmr::InterfaceFactory::make_unique<
        typename Spec::Interface>(PmrAllocator<typename Spec::Concrete>{&memory}, std::forward<Args>(args)...);
}

}  // namespace detail

}  // namespace libcyphal

#endif  // LIBCYPHAL_TYPES_HPP_INCLUDED
