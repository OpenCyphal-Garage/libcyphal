/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_MULTIPLEXER_MOCK_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_MULTIPLEXER_MOCK_HPP_INCLUDED

#include <libcyphal/transport/multiplexer.hpp>

#include <gmock/gmock.h>

namespace libcyphal
{
namespace transport
{

class MultiplexerMock : public IMultiplexer
{
public:
    MultiplexerMock()          = default;
    virtual ~MultiplexerMock() = default;
};

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_MULTIPLEXER_MOCK_HPP_INCLUDED
