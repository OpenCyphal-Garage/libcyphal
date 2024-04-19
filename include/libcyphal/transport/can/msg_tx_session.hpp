/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_CAN_MSG_TX_SESSION_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_CAN_MSG_TX_SESSION_HPP_INCLUDED

#include "delegate.hpp"
#include "libcyphal/transport/msg_sessions.hpp"

#include <canard.h>

#include <numeric>

namespace libcyphal
{
namespace transport
{
namespace can
{
namespace detail
{
class MessageTxSession final : public IMessageTxSession
{
    // In use to disable public construction.
    // See https://seanmiddleditch.github.io/enabling-make-unique-with-private-constructors/
    struct Tag
    {
        explicit Tag()  = default;
        using Interface = IMessageTxSession;
        using Concrete  = MessageTxSession;
    };

public:
    CETL_NODISCARD static Expected<UniquePtr<IMessageTxSession>, AnyError> make(TransportDelegate&     delegate,
                                                                                const MessageTxParams& params)
    {
        auto session = libcyphal::detail::makeUniquePtr<Tag>(delegate.memory(), Tag{}, delegate, params);
        if (session == nullptr)
        {
            return MemoryError{};
        }

        return session;
    }

    MessageTxSession(Tag, TransportDelegate& delegate, const MessageTxParams& params)
        : delegate_{delegate}
        , params_{params}
    {
    }

private:
    // MARK: IMessageTxSession

    CETL_NODISCARD MessageTxParams getParams() const noexcept override
    {
        return params_;
    }

    CETL_NODISCARD cetl::optional<AnyError> send(const TransferMetadata& metadata,
                                                 const PayloadFragments  payload_fragments) override
    {
        std::size_t       payload_size = 0;
        const cetl::byte* payload      = nullptr;
        cetl::byte*       buffer       = nullptr;

        const auto predicate = [&payload_size, &payload](const cetl::span<const cetl::byte> frag) -> bool {
            const auto frag_size = frag.size();
            if (frag_size == 0)
            {
                return false;
            }
            payload = frag.data();
            payload_size += frag_size;
            return true;
        };
        const auto total_fragments = std::count_if(payload_fragments.begin(), payload_fragments.end(), predicate);

        if (total_fragments > 1)
        {
            buffer = static_cast<cetl::byte*>(delegate_.memory().allocate(payload_size));
            if (buffer == nullptr)
            {
                return MemoryError{};
            }

            size_t index = 0;
            for (const auto& frag : payload_fragments)
            {
                std::memcpy(&buffer[index], frag.data(), frag.size());
                index += frag.size();
            }
            payload = buffer;
        }

        // TransferMetadata
        //        TransferId transfer_id;
        //        TimePoint  timestamp;
        //        Priority   priority;

        //        int32_t canardTxPush(CanardTxQueue* const                que,
        //                             CanardInstance* const               ins,
        //                             const CanardMicrosecond             tx_deadline_usec,
        //                             const CanardTransferMetadata* const metadata,
        //                             const size_t                        payload_size,
        //                             const void* const                   payload)
        //
        //        const auto result = canardTxPush(&delegate_.canard_instance(),
        //                                         CanardTransferKindMessage,
        //                                         static_cast<CanardPortID>(params_.subject_id),
        //                                         static_cast<size_t>(metadata.extent_bytes),
        //                                         &transfer_id,
        //                                         &timestamp,
        //                                         &priority,
        //                                         payload_fragments.data(),
        //                                         payload_fragments.size());

        return NotImplementedError{};
    }

    // MARK: IRunnable

    void run(const TimePoint) override {}

    // MARK: Data members:

    TransportDelegate&    delegate_;
    const MessageTxParams params_;

};  // MessageTxSession

}  // namespace detail
}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_TRANSPORT_CAN_MSG_TX_SESSION_HPP_INCLUDED
