/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_SVC_SESSIONS_MOCK_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_SVC_SESSIONS_MOCK_HPP_INCLUDED

#include "unique_ptr_reference_wrapper.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/transport/svc_sessions.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/types.hpp>

#include <gmock/gmock.h>

#include <utility>

namespace libcyphal
{
namespace transport
{

class RequestRxSessionMock : public IRequestRxSession
{
public:
    struct RefWrapper final : UniquePtrReferenceWrapper<IRequestRxSession, RequestRxSessionMock, RefWrapper>
    {
        using UniquePtrReferenceWrapper::UniquePtrReferenceWrapper;

        // MARK: IRequestRxSession

        void setTransferIdTimeout(const Duration timeout) override
        {
            reference().setTransferIdTimeout(timeout);
        }
        RequestRxParams getParams() const noexcept override
        {
            return reference().getParams();
        }
        cetl::optional<ServiceRxTransfer> receive() override
        {
            return reference().receive();
        }
        void setOnReceiveCallback(OnReceiveCallback::Function&& function) override
        {
            reference().setOnReceiveCallback(std::move(function));
        }

    };  // RefWrapper

    RequestRxSessionMock()          = default;
    virtual ~RequestRxSessionMock() = default;

    RequestRxSessionMock(const RequestRxSessionMock&)                = delete;
    RequestRxSessionMock(RequestRxSessionMock&&) noexcept            = delete;
    RequestRxSessionMock& operator=(const RequestRxSessionMock&)     = delete;
    RequestRxSessionMock& operator=(RequestRxSessionMock&&) noexcept = delete;

    MOCK_METHOD(void, setTransferIdTimeout, (const Duration timeout), (override));
    MOCK_METHOD(RequestRxParams, getParams, (), (const, noexcept, override));
    MOCK_METHOD(cetl::optional<ServiceRxTransfer>, receive, (), (override));
    MOCK_METHOD(void, setOnReceiveCallback, (OnReceiveCallback::Function&&), (override));
    MOCK_METHOD(void, deinit, (), (noexcept));  // NOLINT(*-exception-escape)

};  // RequestRxSessionMock

// MARK: -

class RequestTxSessionMock : public IRequestTxSession
{
public:
    struct RefWrapper final : UniquePtrReferenceWrapper<IRequestTxSession, RequestTxSessionMock, RefWrapper>
    {
        using UniquePtrReferenceWrapper::UniquePtrReferenceWrapper;

        // MARK: IRequestTxSession

        RequestTxParams getParams() const noexcept override
        {
            return reference().getParams();
        }
        cetl::optional<AnyFailure> send(const TransferTxMetadata& metadata,
                                        const PayloadFragments    payload_fragments) override
        {
            return reference().send(metadata, payload_fragments);
        }

    };  // RefWrapper

    RequestTxSessionMock()          = default;
    virtual ~RequestTxSessionMock() = default;

    RequestTxSessionMock(const RequestTxSessionMock&)                = delete;
    RequestTxSessionMock(RequestTxSessionMock&&) noexcept            = delete;
    RequestTxSessionMock& operator=(const RequestTxSessionMock&)     = delete;
    RequestTxSessionMock& operator=(RequestTxSessionMock&&) noexcept = delete;

    MOCK_METHOD(RequestTxParams, getParams, (), (const, noexcept, override));
    MOCK_METHOD(cetl::optional<AnyFailure>,
                send,
                (const TransferTxMetadata& metadata, const PayloadFragments payload_fragments),
                (override));
    MOCK_METHOD(void, deinit, (), ());

};  // RequestTxSessionMock

// MARK: -

class ResponseRxSessionMock : public IResponseRxSession
{
public:
    struct RefWrapper final : UniquePtrReferenceWrapper<IResponseRxSession, ResponseRxSessionMock, RefWrapper>
    {
        using UniquePtrReferenceWrapper::UniquePtrReferenceWrapper;

        // MARK: IResponseRxSession

        void setTransferIdTimeout(const Duration timeout) override
        {
            reference().setTransferIdTimeout(timeout);
        }
        ResponseRxParams getParams() const noexcept override
        {
            return reference().getParams();
        }
        cetl::optional<ServiceRxTransfer> receive() override
        {
            return reference().receive();
        }
        void setOnReceiveCallback(OnReceiveCallback::Function&& function) override
        {
            reference().setOnReceiveCallback(std::move(function));
        }

    };  // RefWrapper

    ResponseRxSessionMock()          = default;
    virtual ~ResponseRxSessionMock() = default;

    ResponseRxSessionMock(const ResponseRxSessionMock&)                = delete;
    ResponseRxSessionMock(ResponseRxSessionMock&&) noexcept            = delete;
    ResponseRxSessionMock& operator=(const ResponseRxSessionMock&)     = delete;
    ResponseRxSessionMock& operator=(ResponseRxSessionMock&&) noexcept = delete;

    MOCK_METHOD(void, setTransferIdTimeout, (const Duration timeout), (override));
    MOCK_METHOD(ResponseRxParams, getParams, (), (const, noexcept, override));
    MOCK_METHOD(cetl::optional<ServiceRxTransfer>, receive, (), (override));
    MOCK_METHOD(void, setOnReceiveCallback, (OnReceiveCallback::Function&&), (override));
    MOCK_METHOD(void, deinit, (), (noexcept));  // NOLINT(*-exception-escape)

};  // ResponseRxSessionMock

// MARK: -

class ResponseTxSessionMock : public IResponseTxSession
{
public:
    struct RefWrapper final : UniquePtrReferenceWrapper<IResponseTxSession, ResponseTxSessionMock, RefWrapper>
    {
        using UniquePtrReferenceWrapper::UniquePtrReferenceWrapper;

        // MARK: IResponseTxSession

        ResponseTxParams getParams() const noexcept override
        {
            return reference().getParams();
        }
        cetl::optional<AnyFailure> send(const ServiceTxMetadata& metadata,
                                        const PayloadFragments   payload_fragments) override
        {
            return reference().send(metadata, payload_fragments);
        }

    };  // RefWrapper

    ResponseTxSessionMock()          = default;
    virtual ~ResponseTxSessionMock() = default;

    ResponseTxSessionMock(const ResponseTxSessionMock&)                = delete;
    ResponseTxSessionMock(ResponseTxSessionMock&&) noexcept            = delete;
    ResponseTxSessionMock& operator=(const ResponseTxSessionMock&)     = delete;
    ResponseTxSessionMock& operator=(ResponseTxSessionMock&&) noexcept = delete;

    MOCK_METHOD(ResponseTxParams, getParams, (), (const, noexcept, override));
    MOCK_METHOD(cetl::optional<AnyFailure>,
                send,
                (const ServiceTxMetadata& metadata, const PayloadFragments payload_fragments),
                (override));
    MOCK_METHOD(void, deinit, (), ());

};  // ResponseTxSessionMock

}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_SVC_SESSIONS_MOCK_HPP_INCLUDED
