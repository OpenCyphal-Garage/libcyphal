/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_CAN_DELEGATE_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_DELEGATE_HPP_INCLUDED

#include <canard.h>

namespace libcyphal
{
namespace transport
{
namespace can
{
namespace detail
{

struct TransportDelegate
{
    explicit TransportDelegate(cetl::pmr::memory_resource& memory, CanardInstance canard_instance)
        : memory_{memory}
        , canard_instance_(canard_instance)
    {
    }

    static cetl::optional<AnyError> anyErrorFromCanard(const int8_t result)
    {
        if (result == CANARD_ERROR_INVALID_ARGUMENT)
        {
            return ArgumentError{};
        }
        if (result == CANARD_ERROR_OUT_OF_MEMORY)
        {
            return MemoryError{};
        }

        return {};
    }

    cetl::pmr::memory_resource& memory_;
    CanardInstance              canard_instance_;
};

}  // namespace detail
}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_DELEGATE_HPP_INCLUDED
