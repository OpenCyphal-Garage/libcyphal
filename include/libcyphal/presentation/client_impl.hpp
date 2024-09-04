/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_CLIENT_IMPL_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_CLIENT_IMPL_HPP_INCLUDED

#include "presentation_delegate.hpp"
#include "shared_object.hpp"

#include "libcyphal/common/cavl/cavl.hpp"
#include "libcyphal/executor.hpp"
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
    /// No Sonar cpp:S4963 "The "Rule-of-Zero" should be followed"
    /// b/c we do directly handle resources here.
    ///
    class CallbackNode : public Node<CallbackNode>  // NOSONAR cpp:S4963
    {
    public:
        using Node::isLinked;

        CallbackNode(const CallbackNode&)                = delete;
        CallbackNode& operator=(const CallbackNode&)     = delete;
        CallbackNode& operator=(CallbackNode&&) noexcept = delete;

        transport::TransferId getTransferId() const noexcept
        {
            return transfer_id_;
        }

        TimePoint getResponseDeadline() const noexcept
        {
            return response_deadline_;
        }

        void setResponseDeadline(const TimePoint response_deadline) noexcept
        {
            response_deadline_ = response_deadline;
        }

        CETL_NODISCARD std::int8_t compareByTransferId(const transport::TransferId transfer_id) const noexcept
        {
            if (transfer_id == transfer_id_)
            {
                return 0;
            }
            return (transfer_id > transfer_id_) ? +1 : -1;
        }

        virtual void onResponseTimeout(const TimePoint deadline, const TimePoint approx_now)                  = 0;
        virtual void onResponseRxTransfer(transport::ServiceRxTransfer& transfer, const TimePoint approx_now) = 0;

    protected:
        explicit CallbackNode(const transport::TransferId transfer_id, const TimePoint response_deadline)
            : transfer_id_{transfer_id}
            , response_deadline_{response_deadline}
        {
        }

        ~CallbackNode()                       = default;
        CallbackNode(CallbackNode&&) noexcept = default;

    private:
        // MARK: Data members:

        transport::TransferId transfer_id_;
        TimePoint             response_deadline_;

    };  // CallbackNode

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
        , next_transfer_id_{0}
    {
        CETL_DEBUG_ASSERT(svc_request_tx_session_ != nullptr, "");
        CETL_DEBUG_ASSERT(svc_response_rx_session_ != nullptr, "");

        svc_response_rx_session_->setOnReceiveCallback([this](const auto& arg) {
            //
            onResponseRxTransfer(arg.transfer);
        });
        nearest_deadline_callback_ = executor_.registerCallback([](const auto&) {
            //
            // TODO: implement!
        });

        // TODO: delete this line
        (void) memory_;
    }

    CETL_NODISCARD TimePoint now() const noexcept
    {
        return executor_.now();
    }

    CETL_NODISCARD cetl::pmr::memory_resource& memory() const noexcept
    {
        return memory_;
    }

    CETL_NODISCARD cetl::optional<transport::TransferId> allocateTransferId()
    {
        // TODO: Implement allocation algorithm which takes into account transfer id modulo.
        // For now just increment, which works totally fine in case of UDP transport.
        return next_transfer_id_++;
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

    void retainCallbackNode(CallbackNode& callback_node) noexcept
    {
        CETL_DEBUG_ASSERT(!callback_node.isLinked(), "");

        retain();

        const auto cb_node_existing = cb_nodes_by_transfer_id_.search(                       //
            [transfer_id = callback_node.getTransferId()](const CallbackNode& other_node) {  // predicate
                //
                return other_node.compareByTransferId(transfer_id);
            },
            [&callback_node]() { return &callback_node; });  // "factory"

        (void) cb_node_existing;
        CETL_DEBUG_ASSERT(!std::get<1>(cb_node_existing), "Unexpected existing callback node.");
        CETL_DEBUG_ASSERT(&callback_node == std::get<0>(cb_node_existing), "Unexpected callback node.");
    }

    cetl::optional<transport::AnyFailure> sendRequestPayload(const transport::TransferTxMetadata& tx_metadata,
                                                             const transport::PayloadFragments    payload) const
    {
        return svc_request_tx_session_->send(tx_metadata, payload);
    }

    void releaseCallbackNode(CallbackNode& callback_node) noexcept
    {
        if (callback_node.isLinked())
        {
            cb_nodes_by_transfer_id_.remove(&callback_node);
        }

        release();
    }

    void release() noexcept override
    {
        SharedObject::release();
        if (getRefCount() == 0)
        {
            CETL_DEBUG_ASSERT(cb_nodes_by_transfer_id_.empty(), "");

            delegate_.releaseClientImpl(this);
        }
    }

private:
    void onResponseRxTransfer(transport::ServiceRxTransfer& transfer)
    {
        const auto transfer_id = transfer.metadata.rx_meta.base.transfer_id;
        if (auto* const callback_node = cb_nodes_by_transfer_id_.search(  //
                [transfer_id](const CallbackNode& other_node) {           // predicate
                    //
                    return other_node.compareByTransferId(transfer_id);
                }))
        {
            cb_nodes_by_transfer_id_.remove(callback_node);

            callback_node->onResponseRxTransfer(transfer, now());
        }
    }

    // MARK: Data members:

    cetl::pmr::memory_resource&                    memory_;
    IPresentationDelegate&                         delegate_;
    IExecutor&                                     executor_;
    const UniquePtr<transport::IRequestTxSession>  svc_request_tx_session_;
    const UniquePtr<transport::IResponseRxSession> svc_response_rx_session_;
    const transport::ResponseRxParams              response_rx_params_;
    transport::TransferId                          next_transfer_id_;
    cavl::Tree<CallbackNode>                       cb_nodes_by_transfer_id_;
    IExecutor::Callback::Any                       nearest_deadline_callback_;

};  // ClientImpl

}  // namespace detail
}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_CLIENT_IMPL_HPP_INCLUDED
