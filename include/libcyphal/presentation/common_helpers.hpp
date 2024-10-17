/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_COMMON_HELPERS_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_COMMON_HELPERS_HPP_INCLUDED

#include "libcyphal/errors.hpp"
#include "libcyphal/transport/scattered_buffer.hpp"

#include <cetl/pf17/cetlpf.hpp>

#include <cassert>  // NOLINT for NUNAVUT_ASSERT
#include <nunavut/support/serialization.hpp>

#include <cstdint>
#include <memory>

namespace libcyphal
{
namespace presentation
{

/// Internal implementation details of the Presentation layer.
/// Not supposed to be used directly by the users of the library.
///
namespace detail
{

using DeserializationFailure = cetl::variant<MemoryError, nunavut::support::Error>;

template <typename Message>
static cetl::optional<DeserializationFailure> tryDeserializePayload(const transport::ScatteredBuffer& payload,
                                                                    cetl::pmr::memory_resource&       memory,
                                                                    Message&                          out_message)
{
    // Make a copy of the scattered buffer into a single contiguous temp buffer.
    //
    // Strictly speaking, we could eliminate PMR allocation here in favor of a fixed-size stack buffer
    // (`Message::_traits_::ExtentBytes`), but this might be dangerous in case of large messages.
    // Maybe some kind of hybrid approach would be better,
    // e.g. stack buffer for small messages and PMR for large ones.
    //
    const std::unique_ptr<cetl::byte, PmrRawBytesDeleter>
        tmp_buffer{static_cast<cetl::byte*>(memory.allocate(payload.size())),  // NOSONAR cpp:S5356 cpp:S5357
                   {payload.size(), &memory}};
    if (!tmp_buffer)
    {
        return MemoryError{};
    }
    const auto data_size = payload.copy(0, tmp_buffer.get(), payload.size());

    const auto* const data_raw = static_cast<const void*>(tmp_buffer.get());
    const auto* const data_u8s = static_cast<const std::uint8_t*>(data_raw);  // NOSONAR cpp:S5356 cpp:S5357
    const nunavut::support::const_bitspan bitspan{data_u8s, data_size};

    const nunavut::support::SerializeResult result = deserialize(out_message, bitspan);
    if (!result)
    {
        return result.error();
    }

    return cetl::nullopt;
}

}  // namespace detail
}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_COMMON_HELPERS_HPP_INCLUDED
