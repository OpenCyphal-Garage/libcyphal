/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_CAN_TRANSIENT_ERROR_HANDLER_MOCK_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_TRANSIENT_ERROR_HANDLER_MOCK_HPP_INCLUDED

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/transport/udp/udp_transport.hpp>
#include <libcyphal/transport/errors.hpp>

#include <gmock/gmock.h>

namespace libcyphal
{
namespace transport
{
namespace udp
{

class TransientErrorHandlerMock
{
public:
    cetl::optional<AnyFailure> operator()(IUdpTransport::TransientErrorReport::Variant& report_var)
    {
        return invoke(report_var);
    }

    MOCK_METHOD(cetl::optional<AnyFailure>, invoke, (IUdpTransport::TransientErrorReport::Variant & report_var));
};

}  // namespace udp
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_TRANSIENT_ERROR_HANDLER_MOCK_HPP_INCLUDED
