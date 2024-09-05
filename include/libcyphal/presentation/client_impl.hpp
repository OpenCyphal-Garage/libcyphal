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
    class TimeoutNode : public Node<TimeoutNode>
    {
    public:
        bool isTimeoutLinked() const noexcept
        {
            return isLinked();
        }

        TimePoint getDeadline() const noexcept
        {
            return deadline_;
        }

        void setDeadline(const TimePoint response_deadline) noexcept
        {
            deadline_ = response_deadline;
        }

        CETL_NODISCARD std::int8_t compareByDeadline(const TimePoint deadline) const noexcept
        {
            // No two deadline times compare equal, which allows us to have multiple nodes
            // with the same deadline time in the tree. With two nodes sharing the same deadline time,
            // the one added later is considered to be later.
            return (deadline >= deadline_) ? +1 : -1;
        }

    protected:
        explicit TimeoutNode(const TimePoint deadline)
            : deadline_{deadline}
        {
        }

    private:
        // MARK: Data members:

        TimePoint deadline_;

    };  // TimeoutNode

    /// No Sonar cpp:S4963 'The "Rule-of-Zero" should be followed'
    /// b/c we do directly handle resources here.
    ///
    class CallbackNode : public Node<CallbackNode>, public TimeoutNode  // NOSONAR cpp:S4963
    {
    public:
        CallbackNode(const CallbackNode&)                = delete;
        CallbackNode& operator=(const CallbackNode&)     = delete;
        CallbackNode& operator=(CallbackNode&&) noexcept = delete;

        bool isCallbackLinked() const noexcept
        {
            return Node<CallbackNode>::isLinked();
        }

        transport::TransferId getTransferId() const noexcept
        {
            return transfer_id_;
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
        CallbackNode(const transport::TransferId transfer_id, const TimePoint response_deadline)
            : TimeoutNode{response_deadline}
            , transfer_id_{transfer_id}
        {
        }

        ~CallbackNode()                       = default;
        CallbackNode(CallbackNode&&) noexcept = default;

    private:
        // MARK: Data members:

        transport::TransferId transfer_id_;

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
        , nearest_deadline_{DistantFuture()}
    {
        CETL_DEBUG_ASSERT(svc_request_tx_session_ != nullptr, "");
        CETL_DEBUG_ASSERT(svc_response_rx_session_ != nullptr, "");

        svc_response_rx_session_->setOnReceiveCallback([this](const auto& arg) {
            //
            onResponseRxTransfer(arg.transfer);
        });

        nearest_deadline_callback_ = executor_.registerCallback([this](const auto& arg) {
            //
            onNearestDeadline(arg.approx_now);
        });
        CETL_DEBUG_ASSERT(nearest_deadline_callback_, "Should not fail b/c we pass proper lambda.");

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
        CETL_DEBUG_ASSERT(!callback_node.isCallbackLinked(), "");

        retain();
        insertNewCallbackNode(callback_node);
    }

    cetl::optional<transport::AnyFailure> sendRequestPayload(const transport::TransferTxMetadata& tx_metadata,
                                                             const transport::PayloadFragments    payload) const
    {
        return svc_request_tx_session_->send(tx_metadata, payload);
    }

    void updateDeadlineOfTimeoutNode(TimeoutNode& timeout_node, const TimePoint new_deadline)
    {
        // Remove previous timeout node (if any),
        // and then reinsert the node with updated/given new deadline time.
        //
        CETL_DEBUG_ASSERT(timeout_node.isTimeoutLinked(), "");
        timeout_nodes_by_deadline_.remove(&timeout_node);
        timeout_node.setDeadline(new_deadline);
        insertNewTimeoutNodeAndReschedule(timeout_node);
    }

    void releaseCallbackNode(CallbackNode& callback_node) noexcept
    {
        removeCallbackNode(callback_node);
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
    using Schedule = IExecutor::Callback::Schedule;

    static constexpr TimePoint DistantFuture()
    {
        return TimePoint::max();
    }

    void onResponseRxTransfer(transport::ServiceRxTransfer& transfer)
    {
        const auto transfer_id = transfer.metadata.rx_meta.base.transfer_id;
        if (auto* const callback_node = cb_nodes_by_transfer_id_.search(  //
                [transfer_id](const CallbackNode& other_node) {           // predicate
                    //
                    return other_node.compareByTransferId(transfer_id);
                }))
        {
            removeCallbackNode(*callback_node);
            callback_node->onResponseRxTransfer(transfer, now());
        }
    }

    void onNearestDeadline(const TimePoint approx_now)
    {
        while (auto* const nearest_deadline_node = timeout_nodes_by_deadline_.min())
        {
            if (approx_now < nearest_deadline_node->getDeadline())
            {
                break;
            }

            // Downcast is safe here b/c every timeout node is always a callback node.
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
            auto& callback_node = static_cast<CallbackNode&>(*nearest_deadline_node);

            removeCallbackNode(callback_node);
            callback_node.onResponseTimeout(callback_node.getDeadline(), approx_now);
        }
    }

    void insertNewCallbackNode(CallbackNode& callback_node)
    {
        CETL_DEBUG_ASSERT(!callback_node.isCallbackLinked(), "");

        const auto cb_node_existing = cb_nodes_by_transfer_id_.search(                       //
            [transfer_id = callback_node.getTransferId()](const CallbackNode& other_node) {  // predicate
                //
                return other_node.compareByTransferId(transfer_id);
            },
            [&callback_node]() { return &callback_node; });  // "factory"

        (void) cb_node_existing;
        CETL_DEBUG_ASSERT(callback_node.isCallbackLinked(), "");
        CETL_DEBUG_ASSERT(!std::get<1>(cb_node_existing), "Unexpected existing callback node.");
        CETL_DEBUG_ASSERT(&callback_node == std::get<0>(cb_node_existing), "Unexpected callback node.");

        insertNewTimeoutNodeAndReschedule(callback_node);
    }

    void insertNewTimeoutNodeAndReschedule(TimeoutNode& timeout_node)
    {
        CETL_DEBUG_ASSERT(!timeout_node.isTimeoutLinked(), "");

        const auto new_node_deadline = timeout_node.getDeadline();

        // 1. Insert the new timeout node.
        //
        const std::tuple<TimeoutNode*, bool> timeout_node_existing = timeout_nodes_by_deadline_.search(  //
            [new_node_deadline](const TimeoutNode& other_node) {                                         // predicate
                //
                return other_node.compareByDeadline(new_node_deadline);
            },
            [&timeout_node]() { return &timeout_node; });  // "factory"

        (void) timeout_node_existing;
        CETL_DEBUG_ASSERT(timeout_node.isTimeoutLinked(), "");
        CETL_DEBUG_ASSERT(!std::get<1>(timeout_node_existing), "Unexpected existing timeout node.");
        CETL_DEBUG_ASSERT(&timeout_node == std::get<0>(timeout_node_existing), "Unexpected timeout node.");

        // 2. Reschedule the nearest deadline callback if it's gonna happen earlier than it was before.
        //
        if (nearest_deadline_ > new_node_deadline)
        {
            nearest_deadline_ = new_node_deadline;
            const auto result = nearest_deadline_callback_.schedule(Schedule::Once{new_node_deadline});
            CETL_DEBUG_ASSERT(result, "Should not fail b/c we never reset `nearest_deadline_callback_`.");
            (void) result;
        }
    }

    void removeCallbackNode(CallbackNode& callback_node)
    {
        cb_nodes_by_transfer_id_.remove(&callback_node);
        if (callback_node.isTimeoutLinked())
        {
            removeTimeoutNodeAndReschedule(callback_node);
        }
    }

    void removeTimeoutNodeAndReschedule(TimeoutNode& timeout_node)
    {
        CETL_DEBUG_ASSERT(timeout_node.isTimeoutLinked(), "");

        timeout_nodes_by_deadline_.remove(&timeout_node);
        const auto old_cb_node_deadline = timeout_node.getDeadline();

        // No need to reschedule the nearest deadline callback if deadline of the removed node was not the nearest.
        //
        CETL_DEBUG_ASSERT(old_cb_node_deadline >= nearest_deadline_, "");
        if (nearest_deadline_ < old_cb_node_deadline)
        {
            return;
        }

        if (const auto* const nearest_deadline_node = timeout_nodes_by_deadline_.min())
        {
            // Already existing schedule will work fine if the nearest deadline is not changed.
            //
            if (nearest_deadline_ < nearest_deadline_node->getDeadline())
            {
                nearest_deadline_ = nearest_deadline_node->getDeadline();
                const auto result = nearest_deadline_callback_.schedule(Schedule::Once{nearest_deadline_});
                CETL_DEBUG_ASSERT(result, "Should not fail b/c we never reset `nearest_deadline_callback_`.");
                (void) result;
            }
        }
        else
        {
            // No more timeout nodes left, so cancel the schedule (by moving it to the distant future).
            //
            nearest_deadline_ = DistantFuture();
            const auto result = nearest_deadline_callback_.schedule(Schedule::Once{nearest_deadline_});
            CETL_DEBUG_ASSERT(result, "Should not fail b/c we never reset `nearest_deadline_callback_`.");
            (void) result;
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
    TimePoint                                      nearest_deadline_;
    cavl::Tree<TimeoutNode>                        timeout_nodes_by_deadline_;
    IExecutor::Callback::Any                       nearest_deadline_callback_;

};  // ClientImpl

}  // namespace detail
}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_CLIENT_IMPL_HPP_INCLUDED
