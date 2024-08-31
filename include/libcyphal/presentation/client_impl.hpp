/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_CLIENT_IMPL_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_CLIENT_IMPL_HPP_INCLUDED

#include "presentation_delegate.hpp"
#include "shared_object.hpp"

#include "libcyphal/transport/svc_sessions.hpp"
#include "libcyphal/transport/types.hpp"
#include "libcyphal/types.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>

#include <cstdint>
#include <utility>

namespace libcyphal
{
namespace presentation
{

/// Internal implementation details of the Presentation layer.
/// Not supposed to be used directly by the users of the library.
///
namespace detail
{

class ClientImpl final : public cavl::Node<ClientImpl>, public SharedObject
{
public:
    ClientImpl(cetl::pmr::memory_resource&              memory,
               IPresentationDelegate&                   delegate,
               IExecutor&                               executor,
               UniquePtr<transport::IRequestTxSession>  svc_request_tx_session,
               UniquePtr<transport::IResponseRxSession> svc_response_rx_session)
        : memory_{memory}
        , delegate_{delegate}
        , executor_{executor}
        , svc_request_tx_session_{std::move(svc_request_tx_session)}
        , svc_response_rx_session_{std::move(svc_response_rx_session)}
        , response_rx_params_{svc_response_rx_session_->getParams()}
    {
        CETL_DEBUG_ASSERT(svc_request_tx_session_ != nullptr, "");
        CETL_DEBUG_ASSERT(svc_response_rx_session_ != nullptr, "");

        svc_response_rx_session_->setOnReceiveCallback([this](const auto& arg) {
            //
            onResponseRxTransfer(arg.transfer);
        });

        // TODO: delete these lines
        (void) memory_;
        (void) executor_;
    }

    CETL_NODISCARD std::int32_t compareByNodeAndServiceIds(const transport::ResponseRxParams& rx_params) const
    {
        if (response_rx_params_.server_node_id != rx_params.server_node_id)
        {
            return static_cast<std::int32_t>(response_rx_params_.server_node_id) -
                   static_cast<std::int32_t>(rx_params.server_node_id);
        }

        return static_cast<std::int32_t>(response_rx_params_.service_id) -
               static_cast<std::int32_t>(rx_params.service_id);
    }

    void release() noexcept override
    {
        SharedObject::release();
        if (getRefCount() == 0)
        {
            delegate_.releaseClientImpl(this);
        }
    }

private:
    void onResponseRxTransfer(const transport::ServiceRxTransfer&) {}

    // MARK: Data members:

    cetl::pmr::memory_resource&                    memory_;
    IPresentationDelegate&                         delegate_;
    IExecutor&                                     executor_;
    const UniquePtr<transport::IRequestTxSession>  svc_request_tx_session_;
    const UniquePtr<transport::IResponseRxSession> svc_response_rx_session_;
    const transport::ResponseRxParams              response_rx_params_;

};  // ClientImpl

}  // namespace detail
}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_CLIENT_IMPL_HPP_INCLUDED
