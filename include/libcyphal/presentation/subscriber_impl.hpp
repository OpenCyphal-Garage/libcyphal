/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_SUBSCRIBER_IMPL_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_SUBSCRIBER_IMPL_HPP_INCLUDED

#include "presentation_delegate.hpp"
#include "shared_object.hpp"

#include "libcyphal/common/cavl/cavl.hpp"
#include "libcyphal/transport/msg_sessions.hpp"
#include "libcyphal/transport/types.hpp"
#include "libcyphal/types.hpp"

#include <cetl/cetl.hpp>

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
class SubscriberImpl : public cavl::Node<SubscriberImpl>, public SharedObject
{
public:
    explicit SubscriberImpl(IPresentationDelegate& delegate, UniquePtr<transport::IMessageRxSession> msg_rx_session)
        : delegate_{delegate}
        , msg_rx_session_{std::move(msg_rx_session)}
        , subject_id_{msg_rx_session_->getParams().subject_id}
    {
        CETL_DEBUG_ASSERT(msg_rx_session_ != nullptr, "");
    }

    transport::PortId subjectId() const noexcept
    {
        return subject_id_;
    }

    CETL_NODISCARD std::int32_t compareWith(const transport::PortId subject_id) const
    {
        return static_cast<std::int32_t>(subject_id_) - static_cast<std::int32_t>(subject_id);
    }

    // MARK: SharedObject

    /// @brief Decrements the reference count, and deletes this shared subscriber if the count is zero.
    ///
    /// On return from this function, the object may be deleted, so it must not be used anymore.
    ///
    void release() noexcept override
    {
        SharedObject::release();

        if (getRefCount() == 0)
        {
            delegate_.releaseSubscriber(this);
        }
    }

private:
    // MARK: Data members:

    IPresentationDelegate&                        delegate_;
    const UniquePtr<transport::IMessageRxSession> msg_rx_session_;
    const transport::PortId                       subject_id_;

};  // SubscriberImpl

}  // namespace detail
}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_SUBSCRIBER_IMPL_HPP_INCLUDED
