/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "media_mock.hpp"
#include "memory_resource_mock.hpp"
#include "tracking_memory_resource.hpp"
#include "transient_error_handler_mock.hpp"
#include "tx_rx_sockets_mock.hpp"
#include "virtual_time_scheduler.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/errors.hpp>
#include <libcyphal/executor.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/svc_sessions.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/transport/udp/media.hpp>
#include <libcyphal/transport/udp/svc_tx_sessions.hpp>
#include <libcyphal/transport/udp/tx_rx_sockets.hpp>
#include <libcyphal/transport/udp/udp_transport.hpp>
#include <libcyphal/transport/udp/udp_transport_impl.hpp>
#include <libcyphal/types.hpp>
#include <udpard.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <functional>
#include <utility>

namespace
{

using libcyphal::TimePoint;
using libcyphal::UniquePtr;
using libcyphal::MemoryError;
using namespace libcyphal::transport;       // NOLINT This our main concern here in the unit tests.
using namespace libcyphal::transport::udp;  // NOLINT This our main concern here in the unit tests.

using cetl::byte;

using testing::_;
using testing::Eq;
using testing::Truly;
using testing::Invoke;
using testing::Return;
using testing::SizeIs;
using testing::IsEmpty;
using testing::NotNull;
using testing::StrictMock;
using testing::VariantWith;

// https://github.com/llvm/llvm-project/issues/53444
// NOLINTBEGIN(misc-unused-using-decls, misc-include-cleaner)
using std::literals::chrono_literals::operator""s;
using std::literals::chrono_literals::operator""ms;
// NOLINTEND(misc-unused-using-decls, misc-include-cleaner)

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class TestUdpSvcTxSessions : public testing::Test
{
protected:
    void SetUp() override
    {
        EXPECT_CALL(media_mock_, makeTxSocket())  //
            .WillRepeatedly(Invoke([this] {
                return libcyphal::detail::makeUniquePtr<TxSocketMock::RefWrapper::Spec>(mr_, tx_socket_mock_);
            }));

        EXPECT_CALL(tx_socket_mock_, getMtu())  //
            .WillRepeatedly(Return(UDPARD_MTU_DEFAULT));
    }

    void TearDown() override
    {
        EXPECT_THAT(mr_.allocations, IsEmpty());
        EXPECT_THAT(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);
    }

    TimePoint now() const
    {
        return scheduler_.now();
    }

    UniquePtr<IUdpTransport> makeTransport(const MemoryResourcesSpec&   mem_res_spec,
                                           const cetl::optional<NodeId> local_node_id = cetl::nullopt)
    {
        std::array<IMedia*, 1> media_array{&media_mock_};

        auto maybe_transport = udp::makeTransport(mem_res_spec, scheduler_, media_array, 16);
        EXPECT_THAT(maybe_transport, VariantWith<UniquePtr<IUdpTransport>>(NotNull()));
        auto transport = cetl::get<UniquePtr<IUdpTransport>>(std::move(maybe_transport));

        if (local_node_id)
        {
            transport->setLocalNodeId(*local_node_id);
        }

        return transport;
    }

    // MARK: Data members:

    // NOLINTBEGIN
    libcyphal::VirtualTimeScheduler scheduler_{};
    TrackingMemoryResource          mr_;
    StrictMock<MediaMock>           media_mock_{};
    StrictMock<TxSocketMock>        tx_socket_mock_{"TxS1"};
    // NOLINTEND
};

// MARK: - Tests:

TEST_F(TestUdpSvcTxSessions, make_request_session)
{
    auto transport = makeTransport({mr_}, static_cast<NodeId>(0));

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        auto maybe_session = transport->makeRequestTxSession({123, UDPARD_NODE_ID_MAX});
        ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IRequestTxSession>>(NotNull()));
        auto session = cetl::get<UniquePtr<IRequestTxSession>>(std::move(maybe_session));

        EXPECT_THAT(session->getParams().service_id, 123);
        EXPECT_THAT(session->getParams().server_node_id, UDPARD_NODE_ID_MAX);
    });
    scheduler_.scheduleAt(9s, [&](const auto&) {
        //
        EXPECT_CALL(tx_socket_mock_, deinit());
        transport.reset();
        testing::Mock::VerifyAndClearExpectations(&tx_socket_mock_);
    });
    scheduler_.spinFor(10s);
}

TEST_F(TestUdpSvcTxSessions, make_request_fails_due_to_argument_error)
{
    auto transport = makeTransport({mr_}, static_cast<NodeId>(0));

    // Try invalid service id
    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        auto maybe_session = transport->makeRequestTxSession({UDPARD_SERVICE_ID_MAX + 1, 0});
        EXPECT_THAT(maybe_session, VariantWith<AnyFailure>(VariantWith<libcyphal::ArgumentError>(_)));
    });
    // Try invalid server node id
    scheduler_.scheduleAt(2s, [&](const auto&) {
        //
        auto maybe_session = transport->makeRequestTxSession({0, UDPARD_NODE_ID_MAX + 1});
        EXPECT_THAT(maybe_session, VariantWith<AnyFailure>(VariantWith<libcyphal::ArgumentError>(_)));
    });
    scheduler_.scheduleAt(9s, [&](const auto&) {
        //
        EXPECT_CALL(tx_socket_mock_, deinit());
        transport.reset();
        testing::Mock::VerifyAndClearExpectations(&tx_socket_mock_);
    });
    scheduler_.spinFor(10s);
}

TEST_F(TestUdpSvcTxSessions, make_request_fails_due_to_no_memory)
{
    StrictMock<MemoryResourceMock> mr_mock;
    mr_mock.redirectExpectedCallsTo(mr_);

    auto transport = makeTransport({mr_mock}, static_cast<NodeId>(UDPARD_NODE_ID_MAX));

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        // Emulate that there is no memory available for the message session.
        EXPECT_CALL(mr_mock, do_allocate(sizeof(udp::detail::SvcRequestTxSession), _))  //
            .WillOnce(Return(nullptr));

        auto maybe_session = transport->makeRequestTxSession({0x23, 0});
        EXPECT_THAT(maybe_session, VariantWith<AnyFailure>(VariantWith<MemoryError>(_)));
    });
    scheduler_.scheduleAt(9s, [&](const auto&) {
        //
        EXPECT_CALL(tx_socket_mock_, deinit());
        transport.reset();
        testing::Mock::VerifyAndClearExpectations(&tx_socket_mock_);
    });
    scheduler_.spinFor(10s);
}

TEST_F(TestUdpSvcTxSessions, make_request_fails_due_to_media_socket)
{
    using MakeSocketReport = IUdpTransport::TransientErrorReport::MediaMakeTxSocket;

    auto transport = makeTransport({mr_});

    // 1. Transport will fail to make msg TX session b/c media fails to create a TX socket.
    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        EXPECT_CALL(media_mock_, makeTxSocket())  //
            .WillOnce(Return(MemoryError{}));

        auto maybe_tx_session = transport->makeRequestTxSession({0x23, 0});
        EXPECT_THAT(maybe_tx_session, VariantWith<AnyFailure>(VariantWith<MemoryError>(_)));
    });
    // 2. Transport will succeed to make TX session despite the media fails to create a TX socket.
    //    This is b/c transient error handler will be set and will handle the error.
    scheduler_.scheduleAt(2s, [&](const auto&) {
        //
        EXPECT_CALL(media_mock_, makeTxSocket())  //
            .WillOnce(Return(MemoryError{}));

        StrictMock<TransientErrorHandlerMock> handler_mock;
        transport->setTransientErrorHandler(std::ref(handler_mock));
        EXPECT_CALL(handler_mock, invoke(VariantWith<MakeSocketReport>(Truly([&](auto& report) {
                        EXPECT_THAT(report.failure, VariantWith<MemoryError>(_));
                        EXPECT_THAT(report.media_index, 0);
                        EXPECT_THAT(report.culprit, Ref(media_mock_));
                        return true;
                    }))))
            .WillOnce(Return(cetl::nullopt));

        auto maybe_tx_session = transport->makeRequestTxSession({0x23, 0});
        ASSERT_THAT(maybe_tx_session, VariantWith<UniquePtr<IRequestTxSession>>(NotNull()));

        auto session = cetl::get<UniquePtr<IRequestTxSession>>(std::move(maybe_tx_session));
        EXPECT_THAT(session->getParams().service_id, 0x23);
        EXPECT_THAT(transport->getProtocolParams().mtu_bytes, ITxSocket::DefaultMtu);
    });
    scheduler_.scheduleAt(9s, [&](const auto&) {
        //
        transport.reset();
        testing::Mock::VerifyAndClearExpectations(&tx_socket_mock_);
    });
    scheduler_.spinFor(10s);
}

TEST_F(TestUdpSvcTxSessions, send_empty_payload_request)
{
    StrictMock<MemoryResourceMock> fragment_mr_mock;
    fragment_mr_mock.redirectExpectedCallsTo(mr_);

    auto transport = makeTransport({mr_, nullptr, &fragment_mr_mock});

    auto maybe_session = transport->makeRequestTxSession({0x23, 0});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IRequestTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IRequestTxSession>>(std::move(maybe_session));

    const PayloadFragments empty_payload{};
    TransferTxMetadata     metadata{{0x1AF52, Priority::Low}, {}};

    // 1st try anonymous node - should fail without even trying to allocate & send payload.
    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        metadata.deadline = now() + 1s;
        auto failure      = session->send(metadata, empty_payload);
        EXPECT_THAT(failure, Optional(VariantWith<libcyphal::ArgumentError>(_)));
    });
    // 2nd. Try again but now with a valid node id.
    scheduler_.scheduleAt(2s, [&](const auto&) {
        //
        EXPECT_THAT(transport->setLocalNodeId(0x13), Eq(cetl::nullopt));

        // TX item for our payload to send is expected to be de/allocated on the *fragment* memory resource.
        //
        EXPECT_CALL(fragment_mr_mock, do_allocate(_, _))
            .WillOnce([&](std::size_t size_bytes, std::size_t alignment) -> void* {
                return mr_.allocate(size_bytes, alignment);
            });
        EXPECT_CALL(fragment_mr_mock, do_deallocate(_, _, _))
            .WillOnce([&](void* p, std::size_t size_bytes, std::size_t alignment) {
                mr_.deallocate(p, size_bytes, alignment);
            });

        // Emulate that TX socket has not accepted the payload.
        //
        EXPECT_CALL(tx_socket_mock_, send(_, _, _, _))
            .WillOnce(Return(ITxSocket::SendResult::Success{false /* is_accepted */}));
        EXPECT_CALL(tx_socket_mock_, registerCallback(_))  //
            .WillOnce(Invoke([](auto) { return libcyphal::IExecutor::Callback::Any{}; }));

        metadata.deadline = now() + 1s;
        auto failure      = session->send(metadata, empty_payload);
        EXPECT_THAT(failure, Eq(cetl::nullopt));
    });
    scheduler_.scheduleAt(9s, [&](const auto&) {
        //
        EXPECT_CALL(tx_socket_mock_, deinit());
        session.reset();
        transport.reset();
        testing::Mock::VerifyAndClearExpectations(&tx_socket_mock_);
    });
    scheduler_.spinFor(10s);

    // Payload still inside udpard TX queue (b/c TX socket did not accept the payload),
    // but there will be no memory leak b/c we expect that it should be deallocated when the transport is destroyed.
    // See `EXPECT_THAT(mr_.allocations, IsEmpty());` at the `TearDown` method.
}

TEST_F(TestUdpSvcTxSessions, send_empty_payload_responce)
{
    StrictMock<MemoryResourceMock> fragment_mr_mock;
    fragment_mr_mock.redirectExpectedCallsTo(mr_);

    auto transport = makeTransport({mr_, nullptr, &fragment_mr_mock});

    auto maybe_session = transport->makeResponseTxSession({0x23});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IResponseTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IResponseTxSession>>(std::move(maybe_session));

    const PayloadFragments empty_payload{};
    ServiceTxMetadata      metadata{{{0x1AF52, Priority::Low}, {}}, 0x31};

    // 1st try anonymous node - should fail without even trying to allocate & send payload.
    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        metadata.tx_meta.deadline = now() + 1s;
        auto failure              = session->send(metadata, empty_payload);
        EXPECT_THAT(failure, Optional(VariantWith<libcyphal::ArgumentError>(_)));
    });
    // 2nd. Try again but now with a valid node id.
    scheduler_.scheduleAt(2s, [&](const auto&) {
        //
        EXPECT_THAT(transport->setLocalNodeId(0x13), Eq(cetl::nullopt));

        // TX item for our payload to send is expected to be de/allocated on the *fragment* memory resource.
        //
        EXPECT_CALL(fragment_mr_mock, do_allocate(_, _))
            .WillOnce([&](std::size_t size_bytes, std::size_t alignment) -> void* {
                return mr_.allocate(size_bytes, alignment);
            });
        EXPECT_CALL(fragment_mr_mock, do_deallocate(_, _, _))
            .WillOnce([&](void* p, std::size_t size_bytes, std::size_t alignment) {
                mr_.deallocate(p, size_bytes, alignment);
            });

        // Emulate that TX socket has not accepted the payload.
        //
        EXPECT_CALL(tx_socket_mock_, send(_, _, _, _))
            .WillOnce(Return(ITxSocket::SendResult::Success{false /* is_accepted */}));
        EXPECT_CALL(tx_socket_mock_, registerCallback(_))  //
            .WillOnce(Invoke([](auto) { return libcyphal::IExecutor::Callback::Any{}; }));

        metadata.tx_meta.deadline = now() + 1s;
        auto failure              = session->send(metadata, empty_payload);
        EXPECT_THAT(failure, Eq(cetl::nullopt));
    });
    scheduler_.scheduleAt(9s, [&](const auto&) {
        //
        session.reset();
        EXPECT_CALL(tx_socket_mock_, deinit());
        transport.reset();
        testing::Mock::VerifyAndClearExpectations(&tx_socket_mock_);
    });
    scheduler_.spinFor(10s);

    // Payload still inside udpard TX queue (b/c TX socket did not accept the payload),
    // but there will be no memory leak b/c we expect that it should be deallocated when the transport is destroyed.
    // See `EXPECT_THAT(mr_.allocations, IsEmpty());` at the `TearDown` method.
}

TEST_F(TestUdpSvcTxSessions, send_request)
{
    auto transport = makeTransport({mr_}, NodeId{13});

    auto maybe_session = transport->makeRequestTxSession({0x7B, 0x1F});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IRequestTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IRequestTxSession>>(std::move(maybe_session));

    constexpr auto timeout = 100ms;

    const PayloadFragments empty_payload{};
    TransferTxMetadata     metadata{{0x66, Priority::Slow}, {}};

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        EXPECT_CALL(tx_socket_mock_, send(_, _, _, _))
            .WillOnce([&](auto deadline, auto endpoint, auto dscp, auto fragments) {
                EXPECT_THAT(now(), metadata.deadline - timeout);
                EXPECT_THAT(deadline, metadata.deadline);
                EXPECT_THAT(endpoint.ip_address, 0xEF01001F);
                EXPECT_THAT(endpoint.udp_port, 9382);
                EXPECT_THAT(dscp, 0x0);
                EXPECT_THAT(fragments, SizeIs(1));
                EXPECT_THAT(fragments[0], SizeIs(24 + 4));
                return ITxSocket::SendResult::Success{true /* is_accepted */};
            });
        EXPECT_CALL(tx_socket_mock_, registerCallback(_))  //
            .WillOnce(Invoke([&](auto function) {          //
                return scheduler_.registerAndScheduleNamedCallback("", now() + 10ms, std::move(function));
            }));

        metadata.deadline = now() + timeout;
        auto failure      = session->send(metadata, empty_payload);
        EXPECT_THAT(failure, Eq(cetl::nullopt));
    });
    scheduler_.scheduleAt(9s, [&](const auto&) {
        //
        session.reset();
        EXPECT_CALL(tx_socket_mock_, deinit());
        transport.reset();
        testing::Mock::VerifyAndClearExpectations(&tx_socket_mock_);
    });
    scheduler_.spinFor(10s);
}

TEST_F(TestUdpSvcTxSessions, send_request_with_argument_error)
{
    // Make initially anonymous node transport.
    //
    std::array<IMedia*, 1> media_array{&media_mock_};
    auto                   maybe_transport = udp::makeTransport({mr_}, scheduler_, media_array, 2);
    ASSERT_THAT(maybe_transport, VariantWith<UniquePtr<IUdpTransport>>(NotNull()));
    auto transport = cetl::get<UniquePtr<IUdpTransport>>(std::move(maybe_transport));

    auto maybe_session = transport->makeRequestTxSession({123, 0x1F});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IRequestTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IRequestTxSession>>(std::move(maybe_session));

    const PayloadFragments empty_payload{};
    TransferTxMetadata     metadata{{0x66, Priority::Immediate}, {}};

    // Should fail due to anonymous node.
    scheduler_.scheduleAt(100ms, [&](const auto&) {
        //
        metadata.deadline  = now() + 1s;
        const auto failure = session->send(metadata, empty_payload);
        EXPECT_THAT(failure, Optional(VariantWith<libcyphal::ArgumentError>(_)));
    });
    // Fix anonymous node
    scheduler_.scheduleAt(200ms, [&](const auto&) {
        //
        EXPECT_THAT(transport->setLocalNodeId(13), Eq(cetl::nullopt));

        EXPECT_CALL(tx_socket_mock_, send(_, _, _, _))  //
            .WillOnce([&](auto, auto endpoint, auto, auto) {
                EXPECT_THAT(endpoint.ip_address, 0xEF01001F);
                return ITxSocket::SendResult::Success{true /* is_accepted */};
            });
        EXPECT_CALL(tx_socket_mock_, registerCallback(_))  //
            .WillOnce(Invoke([&](auto function) {          //
                return scheduler_.registerAndScheduleNamedCallback("", now() + 10ms, std::move(function));
            }));

        metadata.deadline  = now() + 1s;
        const auto failure = session->send(metadata, empty_payload);
        EXPECT_THAT(failure, Eq(cetl::nullopt));
    });
    scheduler_.scheduleAt(9s, [&](const auto&) {
        //
        session.reset();
        EXPECT_CALL(tx_socket_mock_, deinit());
        transport.reset();
        testing::Mock::VerifyAndClearExpectations(&tx_socket_mock_);
    });
    scheduler_.spinFor(10s);
}

TEST_F(TestUdpSvcTxSessions, make_response_session)
{
    auto transport = makeTransport({mr_}, NodeId{UDPARD_NODE_ID_MAX});

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        auto maybe_session = transport->makeResponseTxSession({123});
        ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IResponseTxSession>>(NotNull()));
        auto session = cetl::get<UniquePtr<IResponseTxSession>>(std::move(maybe_session));

        EXPECT_THAT(session->getParams().service_id, 123);
    });
    scheduler_.scheduleAt(9s, [&](const auto&) {
        //
        EXPECT_CALL(tx_socket_mock_, deinit());
        transport.reset();
        testing::Mock::VerifyAndClearExpectations(&tx_socket_mock_);
    });
    scheduler_.spinFor(10s);
}

TEST_F(TestUdpSvcTxSessions, make_response_fails_due_to_argument_error)
{
    auto transport = makeTransport({mr_}, NodeId{0});

    // Try invalid service id
    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        auto maybe_session = transport->makeResponseTxSession({UDPARD_SERVICE_ID_MAX + 1});
        EXPECT_THAT(maybe_session, VariantWith<AnyFailure>(VariantWith<libcyphal::ArgumentError>(_)));
    });
    scheduler_.scheduleAt(9s, [&](const auto&) {
        //
        EXPECT_CALL(tx_socket_mock_, deinit());
        transport.reset();
        testing::Mock::VerifyAndClearExpectations(&tx_socket_mock_);
    });
    scheduler_.spinFor(10s);
}

TEST_F(TestUdpSvcTxSessions, make_response_fails_due_to_no_memory)
{
    StrictMock<MemoryResourceMock> mr_mock;
    mr_mock.redirectExpectedCallsTo(mr_);

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        // Emulate that there is no memory available for the message session.
        EXPECT_CALL(mr_mock, do_allocate(sizeof(udp::detail::SvcRequestTxSession), _))  //
            .WillOnce(Return(nullptr));

        auto transport = makeTransport({mr_mock}, NodeId{UDPARD_NODE_ID_MAX});

        auto maybe_session = transport->makeResponseTxSession({0x23});
        EXPECT_THAT(maybe_session, VariantWith<AnyFailure>(VariantWith<MemoryError>(_)));

        EXPECT_CALL(tx_socket_mock_, deinit());
        transport.reset();
        testing::Mock::VerifyAndClearExpectations(&tx_socket_mock_);
    });
    scheduler_.spinFor(10s);
}

TEST_F(TestUdpSvcTxSessions, make_response_fails_due_to_media_socket)
{
    using MakeSocketReport = IUdpTransport::TransientErrorReport::MediaMakeTxSocket;

    auto transport = makeTransport({mr_});

    // 1. Transport will fail to make msg TX session b/c media fails to create a TX socket.
    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        EXPECT_CALL(media_mock_, makeTxSocket())  //
            .WillOnce(Return(MemoryError{}));

        auto maybe_tx_session = transport->makeResponseTxSession({123});
        EXPECT_THAT(maybe_tx_session, VariantWith<AnyFailure>(VariantWith<MemoryError>(_)));
    });
    // 2. Transport will succeed to make TX session despite the media fails to create a TX socket.
    //    This is b/c transient error handler will be set and will handle the error.
    scheduler_.scheduleAt(2s, [&](const auto&) {
        //
        EXPECT_CALL(media_mock_, makeTxSocket())  //
            .WillOnce(Return(MemoryError{}));

        StrictMock<TransientErrorHandlerMock> handler_mock;
        transport->setTransientErrorHandler(std::ref(handler_mock));
        EXPECT_CALL(handler_mock, invoke(VariantWith<MakeSocketReport>(Truly([&](auto& report) {
                        EXPECT_THAT(report.failure, VariantWith<MemoryError>(_));
                        EXPECT_THAT(report.media_index, 0);
                        EXPECT_THAT(report.culprit, Ref(media_mock_));
                        return true;
                    }))))
            .WillOnce(Return(cetl::nullopt));

        auto maybe_tx_session = transport->makeResponseTxSession({123});
        ASSERT_THAT(maybe_tx_session, VariantWith<UniquePtr<IResponseTxSession>>(NotNull()));

        auto session = cetl::get<UniquePtr<IResponseTxSession>>(std::move(maybe_tx_session));
        EXPECT_THAT(session->getParams().service_id, 123);
        EXPECT_THAT(transport->getProtocolParams().mtu_bytes, ITxSocket::DefaultMtu);
    });
    scheduler_.scheduleAt(9s, [&](const auto&) {
        //
        transport.reset();
        testing::Mock::VerifyAndClearExpectations(&tx_socket_mock_);
    });
    scheduler_.spinFor(10s);
}

TEST_F(TestUdpSvcTxSessions, send_response)
{
    auto transport = makeTransport({mr_}, NodeId{0x1F});

    auto maybe_session = transport->makeResponseTxSession({123});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IResponseTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IResponseTxSession>>(std::move(maybe_session));

    constexpr auto timeout = 100ms;

    const PayloadFragments empty_payload{};
    ServiceTxMetadata      metadata{{{0x66, Priority::Fast}, {}}, 0x0D};

    scheduler_.scheduleAt(1s, [&](const auto&) {
        //
        EXPECT_CALL(tx_socket_mock_, send(_, _, _, _))
            .WillOnce([&](auto deadline, auto endpoint, auto dscp, auto fragments) {
                EXPECT_THAT(now(), metadata.tx_meta.deadline - timeout);
                EXPECT_THAT(deadline, metadata.tx_meta.deadline);
                EXPECT_THAT(endpoint.ip_address, 0xEF01000D);
                EXPECT_THAT(endpoint.udp_port, 9382);
                EXPECT_THAT(dscp, 0x0);
                EXPECT_THAT(fragments, SizeIs(1));
                EXPECT_THAT(fragments[0], SizeIs(24 + 4));
                return ITxSocket::SendResult::Success{true /* is_accepted */};
            });
        EXPECT_CALL(tx_socket_mock_, registerCallback(_))  //
            .WillOnce(Invoke([&](auto function) {          //
                return scheduler_.registerAndScheduleNamedCallback("", now() + 10ms, std::move(function));
            }));

        metadata.tx_meta.deadline = now() + timeout;
        auto failure              = session->send(metadata, empty_payload);
        EXPECT_THAT(failure, Eq(cetl::nullopt));
    });
    scheduler_.scheduleAt(9s, [&](const auto&) {
        //
        session.reset();
        EXPECT_CALL(tx_socket_mock_, deinit());
        transport.reset();
        testing::Mock::VerifyAndClearExpectations(&tx_socket_mock_);
    });
    scheduler_.spinFor(10s);
}

TEST_F(TestUdpSvcTxSessions, send_response_with_argument_error)
{
    // Make initially anonymous node transport.
    //
    std::array<IMedia*, 1> media_array{&media_mock_};
    auto                   maybe_transport = udp::makeTransport({mr_}, scheduler_, media_array, 2);
    ASSERT_THAT(maybe_transport, VariantWith<UniquePtr<IUdpTransport>>(NotNull()));
    auto transport = cetl::get<UniquePtr<IUdpTransport>>(std::move(maybe_transport));

    auto maybe_session = transport->makeResponseTxSession({123});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IResponseTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IResponseTxSession>>(std::move(maybe_session));

    const PayloadFragments empty_payload{};
    ServiceTxMetadata      metadata{{{0x66, Priority::Immediate}, {}}, 13};

    // Should fail due to anonymous node.
    scheduler_.scheduleAt(100ms, [&](const auto&) {
        //
        metadata.tx_meta.deadline = now() + 1s;
        const auto failure        = session->send(metadata, empty_payload);
        EXPECT_THAT(failure, Optional(VariantWith<libcyphal::ArgumentError>(_)));
    });
    // Fix anonymous node, but break remote node id.
    scheduler_.scheduleAt(200ms, [&](const auto&) {
        //
        EXPECT_THAT(transport->setLocalNodeId(31), Eq(cetl::nullopt));

        metadata.tx_meta.deadline = now() + 1s;
        metadata.remote_node_id   = UDPARD_NODE_ID_MAX + 1;
        const auto maybe_error    = session->send(metadata, empty_payload);
        EXPECT_THAT(maybe_error, Optional(VariantWith<libcyphal::ArgumentError>(_)));
    });
    scheduler_.scheduleAt(9s, [&](const auto&) {
        //
        session.reset();
        EXPECT_CALL(tx_socket_mock_, deinit());
        transport.reset();
        testing::Mock::VerifyAndClearExpectations(&tx_socket_mock_);
    });
    scheduler_.spinFor(10s);
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
