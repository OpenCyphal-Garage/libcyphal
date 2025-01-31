/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_TRANSFER_ID_MAP_MOCK_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_TRANSFER_ID_MAP_MOCK_HPP_INCLUDED

#include <libcyphal/transport/transfer_id_map.hpp>

#include <gmock/gmock.h>

namespace libcyphal
{
namespace transport
{

class TransferIdMapMock : public ITransferIdMap
{
public:
    TransferIdMapMock()          = default;
    virtual ~TransferIdMapMock() = default;

    TransferIdMapMock(const TransferIdMapMock&)                = delete;
    TransferIdMapMock(TransferIdMapMock&&) noexcept            = delete;
    TransferIdMapMock& operator=(const TransferIdMapMock&)     = delete;
    TransferIdMapMock& operator=(TransferIdMapMock&&) noexcept = delete;

    MOCK_METHOD(TransferId, getIdFor, (const SessionSpec& session_spec), (const, noexcept, override));
    MOCK_METHOD(void, setIdFor, (const SessionSpec& session_spec, const TransferId transfer_id), (noexcept, override));

};  // TransferIdMapMock

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_TRANSFER_ID_MAP_MOCK_HPP_INCLUDED
