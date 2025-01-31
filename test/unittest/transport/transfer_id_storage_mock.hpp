/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_TRANSFER_ID_STORAGE_MOCK_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_TRANSFER_ID_STORAGE_MOCK_HPP_INCLUDED

#include <libcyphal/transport/transfer_id_map.hpp>

#include <gmock/gmock.h>

namespace libcyphal
{
namespace transport
{
namespace detail
{

class TransferIdStorageMock : public ITransferIdStorage
{
public:
    TransferIdStorageMock()          = default;
    virtual ~TransferIdStorageMock() = default;

    TransferIdStorageMock(const TransferIdStorageMock&)                = delete;
    TransferIdStorageMock(TransferIdStorageMock&&) noexcept            = delete;
    TransferIdStorageMock& operator=(const TransferIdStorageMock&)     = delete;
    TransferIdStorageMock& operator=(TransferIdStorageMock&&) noexcept = delete;

    MOCK_METHOD(TransferId, load, (), (const, noexcept, override));
    MOCK_METHOD(void, save, (const TransferId transfer_id), (noexcept, override));

};  // TransferIdStorageMock

}  // namespace detail
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_TRANSFER_ID_STORAGE_MOCK_HPP_INCLUDED
