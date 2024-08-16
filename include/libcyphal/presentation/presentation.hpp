/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_PRESENTATION_PRESENTATION_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_PRESENTATION_HPP_INCLUDED

#include "libcyphal/common/cavl/cavl.hpp"
#include "libcyphal/transport/errors.hpp"
#include "libcyphal/transport/transport.hpp"
#include "libcyphal/transport/types.hpp"
#include "presentation_delegate.hpp"
#include "publisher.hpp"
#include "publisher_impl.hpp"

#include <cetl/pf17/cetlpf.hpp>

#include <utility>

namespace libcyphal
{
namespace presentation
{

// TODO: docs
class Presentation final : public detail::IPresentationDelegate
{
public:
    Presentation(cetl::pmr::memory_resource& memory, transport::ITransport& transport) noexcept
        : memory_{memory}
        , transport_{transport}
        , publisher_nodes_allocator_{&memory_}
    {
    }

    /// @brief Makes a message publisher.
    ///
    /// The publisher must never outlive this presentation object.
    ///
    /// @tparam Message DSDL compiled (aka Nunavut generated) type of the message to publish.
    /// @param port_id The port ID to publish the message on.
    ///
    template <typename Message>
    Expected<Publisher<Message>, transport::AnyFailure> makePublisher(const transport::PortId port_id)
    {
        cetl::optional<transport::AnyFailure> make_session_failure;

        const auto publisher_existing = publisher_impl_nodes_.search(
            [port_id](const detail::PublisherImpl& other_pub) {  // predicate
                //
                return other_pub.compareWith(port_id);
            },
            [this, port_id, &make_session_failure]() -> detail::PublisherImpl* {  // factory
                //
                auto maybe_session = transport_.makeMessageTxSession({port_id});
                if (auto* const failure = cetl::get_if<transport::AnyFailure>(&maybe_session))
                {
                    make_session_failure = std::move(*failure);
                    return nullptr;
                }
                auto msg_tx_session = cetl::get<MessageTxSessionPtr>(std::move(maybe_session));
                if (!msg_tx_session)
                {
                    make_session_failure = transport::MemoryError{};
                    return nullptr;
                }
                return makePublisherImpl(std::move(msg_tx_session));
            });

        if (make_session_failure)
        {
            return std::move(*make_session_failure);
        }

        auto* const publisher_impl = std::get<0>(publisher_existing);
        CETL_DEBUG_ASSERT(publisher_impl != nullptr, "");

        return Publisher<Message>{publisher_impl};
    }

private:
    using MessageTxSessionPtr = UniquePtr<transport::IMessageTxSession>;

    CETL_NODISCARD detail::PublisherImpl* makePublisherImpl(MessageTxSessionPtr&& msg_tx_session)
    {
        CETL_DEBUG_ASSERT(msg_tx_session, "");

        if (auto* const publisher_impl = publisher_nodes_allocator_.allocate(1))
        {
            publisher_nodes_allocator_.construct(publisher_impl, *this, std::move(msg_tx_session));
            return publisher_impl;
        }
        return nullptr;
    }

    // MARK: IPresentationDelegate

    void releasePublisher(detail::PublisherImpl* const publisher_impl) noexcept override
    {
        CETL_DEBUG_ASSERT(publisher_impl, "");

        // TODO: make it async (deferred to "on idle" callback).
        publisher_impl_nodes_.remove(publisher_impl);
        // No Sonar
        // - cpp:S3432   "Destructors should not be called explicitly"
        // - cpp:M23_329 "Advanced memory management" shall not be used"
        // b/c we do our own low-level PMR management here.
        publisher_impl->~PublisherImpl();  // NOSONAR cpp:S3432 cpp:M23_329
        publisher_nodes_allocator_.deallocate(publisher_impl, 1);
    }

    // MARK: Data members:

    cetl::pmr::memory_resource&                            memory_;
    transport::ITransport&                                 transport_;
    cavl::Tree<detail::PublisherImpl>                      publisher_impl_nodes_;
    libcyphal::detail::PmrAllocator<detail::PublisherImpl> publisher_nodes_allocator_;

};  // Presentation

}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_PRESENTATION_HPP_INCLUDED
