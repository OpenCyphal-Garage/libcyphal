/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_COMMON_HELPERS_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_COMMON_HELPERS_HPP_INCLUDED

#include "libcyphal/transport/scattered_buffer.hpp"

#include <cetl/pf17/cetlpf.hpp>

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

constexpr std::size_t SmallPayloadSize = 256;

template <typename Message>
static cetl::optional<DeserializationFailure> tryDeserializePayload(const transport::ScatteredBuffer& payload,
                                                                    cetl::pmr::memory_resource&       memory,
                                                                    Message&                          out_message)
{
    // To reduce heap allocations, we use stack for "small" (<=256 bytes) messages.
    //
    // Strictly speaking, we could eliminate PMR allocation here in favor of a fixed-size stack buffer
    // (using `Message::_traits_::ExtentBytes` as the maximum possible size for the Message).
    // But this might be dangerous (stack overflow!) in case of large messages, so it's done only for small ones.
    //
    if (payload.size() <= SmallPayloadSize)
    {
        std::array<std::uint8_t, SmallPayloadSize> small_buffer;
        const auto                                 data_size = payload.copy(0, small_buffer.data(), payload.size());
        const nunavut::support::const_bitspan      bitspan{small_buffer.data(), data_size};

        const nunavut::support::SerializeResult result = deserialize(out_message, bitspan);
        return result ? cetl::nullopt : cetl::optional<DeserializationFailure>(result.error());
    }

    // Make a copy of the scattered buffer into a single contiguous (PMR allocated) temp buffer.
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
    return result ? cetl::nullopt : cetl::optional<DeserializationFailure>(result.error());
}

}  // namespace detail
}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_COMMON_HELPERS_HPP_INCLUDED
