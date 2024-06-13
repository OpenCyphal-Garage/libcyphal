/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_CAN_TRANSIENT_ERROR_HANDLER_MOCK_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_TRANSIENT_ERROR_HANDLER_MOCK_HPP_INCLUDED

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/transport/can/can_transport_impl.hpp>
#include <libcyphal/transport/errors.hpp>

#include <gmock/gmock.h>

namespace libcyphal
{
namespace transport
{
namespace can
{

class TransientErrorHandlerMock
{
public:
    cetl::optional<AnyError> operator()(ICanTransport::TransientErrorReport::Variant& report_var)
    {
        return invoke(report_var);
    }

    MOCK_METHOD(cetl::optional<AnyError>, invoke, (ICanTransport::TransientErrorReport::Variant& report_var));
};

}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_TRANSIENT_ERROR_HANDLER_MOCK_HPP_INCLUDED
