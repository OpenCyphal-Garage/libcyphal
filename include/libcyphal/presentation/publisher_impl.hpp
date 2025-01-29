/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_PUBLISHER_IMPL_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_PUBLISHER_IMPL_HPP_INCLUDED

#include "presentation_delegate.hpp"
#include "shared_object.hpp"

#include "libcyphal/common/cavl/cavl.hpp"
#include "libcyphal/transport/errors.hpp"
#include "libcyphal/transport/msg_sessions.hpp"
#include "libcyphal/transport/transfer_id_map.hpp"
#include "libcyphal/transport/types.hpp"
#include "libcyphal/types.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>

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

class PublisherImpl final : public common::cavl::Node<PublisherImpl>, public SharedObject
{
public:
    using Node::remove;
    using Node::isLinked;

    PublisherImpl(IPresentationDelegate& delegate, UniquePtr<transport::IMessageTxSession> msg_tx_session)
        : delegate_{delegate}
        , msg_tx_session_{std::move(msg_tx_session)}
        , subject_id_{msg_tx_session_->getParams().subject_id}
        , next_transfer_id_{0}
    {
        CETL_DEBUG_ASSERT(msg_tx_session_ != nullptr, "");

        if (const auto* const transfer_id_map = delegate.getTransferIdMap())
        {
            if (const auto local_node_id = delegate.getLocalNodeId())
            {
                const SessionSpec session_spec{subject_id_, local_node_id.value()};
                next_transfer_id_ = transfer_id_map->getIdFor(session_spec);
            }
        }
    }

    CETL_NODISCARD cetl::pmr::memory_resource& memory() const noexcept
    {
        return delegate_.memory();
    }

    CETL_NODISCARD std::int32_t compareBySubjectId(const transport::PortId subject_id) const
    {
        return static_cast<std::int32_t>(subject_id_) - static_cast<std::int32_t>(subject_id);
    }

    cetl::optional<transport::AnyFailure> publishRawData(const TimePoint                   deadline,
                                                         const transport::Priority         priority,
                                                         const transport::PayloadFragments payload_fragments)
    {
        next_transfer_id_ += 1;
        const transport::TransferTxMetadata metadata{{next_transfer_id_, priority}, deadline};

        return msg_tx_session_->send(metadata, payload_fragments);
    }

    // MARK: SharedObject

    /// @brief Decrements the reference count, and deletes this shared publisher if the count is zero.
    ///
    /// On return from this function, the object may be deleted, so it must not be used anymore.
    ///
    bool release() noexcept override
    {
        if (SharedObject::release())
        {
            delegate_.markSharedObjAsUnreferenced(*this);
            return true;
        }

        return false;
    }

private:
    using SessionSpec = transport::ITransferIdMap::SessionSpec;

    // MARK: SharedObject

    void destroy() noexcept override
    {
        if (auto* const transfer_id_map = delegate_.getTransferIdMap())
        {
            if (const auto local_node_id = delegate_.getLocalNodeId())
            {
                const SessionSpec session_spec{subject_id_, local_node_id.value()};
                transfer_id_map->setIdFor(session_spec, next_transfer_id_);
            }
        }

        delegate_.forgetPublisherImpl(*this);
        destroyWithPmr(this, delegate_.memory());
    }

    // MARK: Data members:

    IPresentationDelegate&                        delegate_;
    const UniquePtr<transport::IMessageTxSession> msg_tx_session_;
    const transport::PortId                       subject_id_;
    transport::TransferId                         next_transfer_id_;

};  // PublisherImpl

}  // namespace detail
}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_PUBLISHER_IMPL_HPP_INCLUDED
