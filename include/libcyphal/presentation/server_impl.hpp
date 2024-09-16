/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_SERVER_IMPL_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_SERVER_IMPL_HPP_INCLUDED

#include "common_helpers.hpp"

#include "libcyphal/time_provider.hpp"
#include "libcyphal/transport/errors.hpp"
#include "libcyphal/transport/scattered_buffer.hpp"
#include "libcyphal/transport/svc_sessions.hpp"
#include "libcyphal/transport/types.hpp"
#include "libcyphal/types.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>

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

class ServerImpl final
{
public:
    class Callback
    {
    public:
        virtual void onRequestRxTransfer(const TimePoint                     approx_now,
                                         const transport::ServiceRxTransfer& rx_transfer) = 0;

        Callback(const Callback& other)                = delete;
        Callback(Callback&& other) noexcept            = delete;
        Callback& operator=(const Callback& other)     = delete;
        Callback& operator=(Callback&& other) noexcept = delete;

    protected:
        Callback()  = default;
        ~Callback() = default;

    };  // Callback

    ServerImpl(cetl::pmr::memory_resource&              memory,
               ITimeProvider&                           time_provider,
               UniquePtr<transport::IRequestRxSession>  svc_req_rx_session,
               UniquePtr<transport::IResponseTxSession> svc_res_tx_session)
        : memory_{memory}
        , time_provider_{time_provider}
        , svc_req_rx_session_{std::move(svc_req_rx_session)}
        , svc_res_tx_session_{std::move(svc_res_tx_session)}
    {
        CETL_DEBUG_ASSERT(svc_req_rx_session_ != nullptr, "");
        CETL_DEBUG_ASSERT(svc_res_tx_session_ != nullptr, "");
    }

    void setOnReceiveCallback(Callback& callback) const
    {
        CETL_DEBUG_ASSERT(svc_req_rx_session_ != nullptr, "");

        const auto& time_provider = time_provider_;
        svc_req_rx_session_->setOnReceiveCallback([&time_provider, &callback](const auto& arg) {
            //
            callback.onRequestRxTransfer(time_provider.now(), arg.transfer);
        });
    }

    cetl::optional<transport::AnyFailure> respondWithPayload(const transport::ServiceTxMetadata& tx_metadata,
                                                             const transport::PayloadFragments   payload) const
    {
        return svc_res_tx_session_->send(tx_metadata, payload);
    }

    template <typename Request>
    bool tryDeserialize(const transport::ScatteredBuffer& buffer, Request& request)
    {
        return tryDeserializePayload(buffer, memory_, request) == cetl::nullopt;
    }

private:
    // MARK: Data members:

    cetl::pmr::memory_resource&              memory_;
    ITimeProvider&                           time_provider_;
    UniquePtr<transport::IRequestRxSession>  svc_req_rx_session_;
    UniquePtr<transport::IResponseTxSession> svc_res_tx_session_;

};  // ServerImpl

}  // namespace detail
}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_SERVER_IMPL_HPP_INCLUDED
