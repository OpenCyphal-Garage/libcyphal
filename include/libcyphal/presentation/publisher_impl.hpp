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

// TODO: docs
class PublisherImpl : public cavl::Node<PublisherImpl>, public SharedObject
{
public:
    explicit PublisherImpl(IPresentationDelegate& delegate, UniquePtr<transport::IMessageTxSession> msg_tx_session)
        : delegate_{delegate}
        , msg_tx_session_{std::move(msg_tx_session)}
        , subject_id_{msg_tx_session_->getParams().subject_id}
        , transfer_id_{0}
    {
        CETL_DEBUG_ASSERT(msg_tx_session_ != nullptr, "");
    }

    transport::PortId subjectId() const noexcept
    {
        return subject_id_;
    }

    CETL_NODISCARD std::int32_t compareWith(const transport::PortId subject_id) const
    {
        return static_cast<std::int32_t>(subject_id_) - static_cast<std::int32_t>(subject_id);
    }

    cetl::optional<transport::AnyFailure> publishRawData(const TimePoint                    deadline,
                                                         const transport::Priority          priority,
                                                         const cetl::span<const cetl::byte> data)
    {
        transfer_id_ += 1;
        const transport::TransferTxMetadata metadata{{transfer_id_, priority}, deadline};

        const std::array<const cetl::span<const cetl::byte>, 1> payload{data};
        return msg_tx_session_->send(metadata, payload);
    }

    // MARK: SharedObject

    /// @brief Decrements the reference count, and deletes this shared publisher if the count is zero.
    ///
    /// On return from this function, the object may be deleted, so it must not be used anymore.
    ///
    void release() noexcept override
    {
        SharedObject::release();

        if (getRefCount() == 0)
        {
            delegate_.releasePublisher(this);
        }
    }

private:
    // MARK: Data members:

    IPresentationDelegate&                        delegate_;
    const UniquePtr<transport::IMessageTxSession> msg_tx_session_;
    const transport::PortId                       subject_id_;
    transport::TransferId                         transfer_id_;

};  // PublisherImpl

}  // namespace detail
}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_PUBLISHER_IMPL_HPP_INCLUDED
