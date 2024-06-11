/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_TRANSIENT_ERROR_HANDLER_MOCK_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_TRANSIENT_ERROR_HANDLER_MOCK_HPP_INCLUDED

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/transport.hpp>

#include <gmock/gmock.h>

namespace libcyphal
{
namespace transport
{

class TransientErrorHandlerMock
{
public:
    cetl::optional<AnyError> operator()(ITransport::AnyErrorReport& report)
    {
        return invoke(report);
    }

    MOCK_METHOD(cetl::optional<AnyError>, invoke, (ITransport::AnyErrorReport& report));
};

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_TRANSIENT_ERROR_HANDLER_MOCK_HPP_INCLUDED
