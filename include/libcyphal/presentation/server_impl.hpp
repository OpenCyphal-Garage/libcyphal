/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_SERVER_IMPL_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_SERVER_IMPL_HPP_INCLUDED

#include <libcyphal/transport/svc_sessions.hpp>

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

        auto& time_provider = time_provider_;
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

    // No Sonar `cpp:S5356` and `cpp:S5357` b/c of raw PMR memory allocation,
    // as well as data pointer type mismatches between scattered buffer and `const_bitspan`.
    // TODO: Eliminate PMR allocation - when `deserialize` will support scattered buffers.
    // TODO: Move this method to some common place (\this `ServerImpl` and `SubscriberImpl` have it).
    template <typename Request>
    bool tryDeserialize(const transport::ScatteredBuffer& buffer, Request& request)
    {
        // Make a copy of the scattered buffer into a single contiguous temp buffer.
        //
        const std::unique_ptr<cetl::byte, PmrRawBytesDeleter>
            tmp_buffer{static_cast<cetl::byte*>(memory_.allocate(buffer.size())),  // NOSONAR cpp:S5356 cpp:S5357,
                       {buffer.size(), &memory_}};
        if (!tmp_buffer)
        {
            return false;
        }
        const auto data_size = buffer.copy(0, tmp_buffer.get(), buffer.size());

        const auto* const data_raw = static_cast<const void*>(tmp_buffer.get());
        const auto* const data_u8s = static_cast<const std::uint8_t*>(data_raw);  // NOSONAR cpp:S5356 cpp:S5357
        const nunavut::support::const_bitspan bitspan{data_u8s, data_size};

        return deserialize(request, bitspan);
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
