/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "../../cetl_gtest_helpers.hpp"
#include "../../memory_resource_mock.hpp"
#include "../../tracking_memory_resource.hpp"
#include "../../virtual_time_scheduler.hpp"
#include "../multiplexer_mock.hpp"
#include "media_mock.hpp"
#include "tx_rx_sockets_mock.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/svc_sessions.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/transport/udp/media.hpp>
#include <libcyphal/transport/udp/svc_tx_sessions.hpp>
#include <libcyphal/transport/udp/udp_transport.hpp>
#include <libcyphal/transport/udp/udp_transport_impl.hpp>
#include <libcyphal/types.hpp>
#include <udpard.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <utility>

namespace
{

using libcyphal::TimePoint;
using libcyphal::UniquePtr;
using namespace libcyphal::transport;       // NOLINT This our main concern here in the unit tests.
using namespace libcyphal::transport::udp;  // NOLINT This our main concern here in the unit tests.

using cetl::byte;

using testing::_;
using testing::Eq;
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
        EXPECT_CALL(media_mock_, getMtu()).WillRepeatedly(Return(UDPARD_MTU_DEFAULT));
        EXPECT_CALL(media_mock_, makeTxSocket()).WillRepeatedly(testing::Invoke([this]() {
            return libcyphal::detail::makeUniquePtr<TxSocketMock::ReferenceWrapper::Spec>(mr_, tx_socket_mock_);
        }));
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

        auto maybe_transport = udp::makeTransport(mem_res_spec, mux_mock_, media_array, 16);
        EXPECT_THAT(maybe_transport, VariantWith<UniquePtr<IUdpTransport>>(NotNull()));
        auto transport = cetl::get<UniquePtr<IUdpTransport>>(std::move(maybe_transport));

        if (local_node_id.has_value())
        {
            transport->setLocalNodeId(local_node_id.value());
        }

        return transport;
    }

    // MARK: Data members:

    // NOLINTBEGIN
    libcyphal::VirtualTimeScheduler scheduler_{};
    TrackingMemoryResource          mr_;
    StrictMock<MultiplexerMock>     mux_mock_{};
    StrictMock<MediaMock>           media_mock_{};
    StrictMock<TxSocketMock>        tx_socket_mock_{};
    // NOLINTEND
};

// MARK: Tests:

TEST_F(TestUdpSvcTxSessions, make_request_session)
{
    auto transport = makeTransport({mr_}, static_cast<NodeId>(0));

    auto maybe_session = transport->makeRequestTxSession({123, UDPARD_NODE_ID_MAX});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IRequestTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IRequestTxSession>>(std::move(maybe_session));

    EXPECT_THAT(session->getParams().service_id, 123);
    EXPECT_THAT(session->getParams().server_node_id, UDPARD_NODE_ID_MAX);

    EXPECT_THAT(session->run(now()), UbVariantWithoutValue());
}

TEST_F(TestUdpSvcTxSessions, make_request_fails_due_to_argument_error)
{
    auto transport = makeTransport({mr_}, static_cast<NodeId>(0));

    // Try invalid service id
    {
        auto maybe_session = transport->makeRequestTxSession({UDPARD_SERVICE_ID_MAX + 1, 0});
        EXPECT_THAT(maybe_session, VariantWith<AnyError>(VariantWith<ArgumentError>(_)));
    }

    // Try invalid server node id
    {
        auto maybe_session = transport->makeRequestTxSession({0, UDPARD_NODE_ID_MAX + 1});
        EXPECT_THAT(maybe_session, VariantWith<AnyError>(VariantWith<ArgumentError>(_)));
    }
}

TEST_F(TestUdpSvcTxSessions, make_request_fails_due_to_no_memory)
{
    StrictMock<MemoryResourceMock> mr_mock{};
    mr_mock.redirectExpectedCallsTo(mr_);

    // Emulate that there is no memory available for the message session.
    EXPECT_CALL(mr_mock, do_allocate(sizeof(udp::detail::SvcRequestTxSession), _)).WillOnce(Return(nullptr));

    auto transport = makeTransport({mr_mock}, static_cast<NodeId>(UDPARD_NODE_ID_MAX));

    auto maybe_session = transport->makeRequestTxSession({0x23, 0});
    EXPECT_THAT(maybe_session, VariantWith<AnyError>(VariantWith<MemoryError>(_)));
}

TEST_F(TestUdpSvcTxSessions, send_empty_payload_request_and_no_transport_run)
{
    StrictMock<MemoryResourceMock> fragment_mr_mock{};
    fragment_mr_mock.redirectExpectedCallsTo(mr_);

    auto transport = makeTransport({mr_, nullptr, &fragment_mr_mock});

    auto maybe_session = transport->makeRequestTxSession({0x23, 0});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IRequestTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IRequestTxSession>>(std::move(maybe_session));

    const PayloadFragments empty_payload{};
    const TransferMetadata metadata{0x1AF52, {}, Priority::Low};

    // 1st try anonymous node - should fail without even trying to allocate & send payload.
    {
        auto maybe_error = session->send(metadata, empty_payload);
        EXPECT_THAT(maybe_error, Optional(VariantWith<ArgumentError>(_)));
    }

    // 2nd. Try again but now with a valid node id.
    {
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

        auto maybe_error = session->send(metadata, empty_payload);
        EXPECT_THAT(maybe_error, Eq(cetl::nullopt));

        scheduler_.runNow(+10ms, [&] { session->run(scheduler_.now()); });
    }

    // Payload still inside udpard TX queue (b/c there was no `transport->run` call deliberately),
    // but there will be no memory leak b/c we expect that it should be deallocated when the transport is destroyed.
    // See `EXPECT_THAT(mr_.allocations, IsEmpty());` at the `TearDown` method.
}

TEST_F(TestUdpSvcTxSessions, send_empty_payload_responce_and_no_transport_run)
{
    StrictMock<MemoryResourceMock> fragment_mr_mock{};
    fragment_mr_mock.redirectExpectedCallsTo(mr_);

    auto transport = makeTransport({mr_, nullptr, &fragment_mr_mock});

    auto maybe_session = transport->makeResponseTxSession({0x23});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IResponseTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IResponseTxSession>>(std::move(maybe_session));

    const PayloadFragments        empty_payload{};
    const ServiceTransferMetadata metadata{0x1AF52, {}, Priority::Low, 0x31};

    // 1st try anonymous node - should fail without even trying to allocate & send payload.
    {
        auto maybe_error = session->send(metadata, empty_payload);
        EXPECT_THAT(maybe_error, Optional(VariantWith<ArgumentError>(_)));
    }

    // 2nd. Try again but now with a valid node id.
    {
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

        auto maybe_error = session->send(metadata, empty_payload);
        EXPECT_THAT(maybe_error, Eq(cetl::nullopt));

        scheduler_.runNow(+10ms, [&] { session->run(scheduler_.now()); });
    }

    // Payload still inside udpard TX queue (b/c there was no `transport->run` call deliberately),
    // but there will be no memory leak b/c we expect that it should be deallocated when the transport is destroyed.
    // See `EXPECT_THAT(mr_.allocations, IsEmpty());` at the `TearDown` method.
}

TEST_F(TestUdpSvcTxSessions, send_request)
{
    auto transport = makeTransport({mr_}, NodeId{13});

    auto maybe_session = transport->makeRequestTxSession({0x7B, 0x1F});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IRequestTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IRequestTxSession>>(std::move(maybe_session));

    scheduler_.runNow(+10s);
    const auto send_time = now();
    const auto timeout   = 100ms;
    session->setSendTimeout(timeout);

    const PayloadFragments empty_payload{};
    const TransferMetadata metadata{0x66, send_time, Priority::Slow};

    auto maybe_error = session->send(metadata, empty_payload);
    EXPECT_THAT(maybe_error, Eq(cetl::nullopt));

    EXPECT_CALL(tx_socket_mock_, send(_, _, _, _))
        .WillOnce([&](auto deadline, auto endpoint, auto dscp, auto fragments) {
            EXPECT_THAT(now(), send_time + 10ms);
            EXPECT_THAT(deadline, send_time + timeout);
            EXPECT_THAT(endpoint.ip_address, 0xEF01001F);
            EXPECT_THAT(endpoint.udp_port, 9382);
            EXPECT_THAT(dscp, 0x0);
            EXPECT_THAT(fragments, SizeIs(1));
            EXPECT_THAT(fragments[0], SizeIs(24 + 4));
            return true;
        });

    scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
}

TEST_F(TestUdpSvcTxSessions, send_request_with_argument_error)
{
    // Make initially anonymous node transport.
    //
    std::array<IMedia*, 1> media_array{&media_mock_};
    auto                   maybe_transport = udp::makeTransport({mr_}, mux_mock_, media_array, 2);
    ASSERT_THAT(maybe_transport, VariantWith<UniquePtr<IUdpTransport>>(NotNull()));
    auto transport = cetl::get<UniquePtr<IUdpTransport>>(std::move(maybe_transport));

    auto maybe_session = transport->makeRequestTxSession({123, 0x1F});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IRequestTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IRequestTxSession>>(std::move(maybe_session));

    scheduler_.runNow(+100ms);
    const auto timeout       = 1s;
    const auto transfer_time = now();

    const PayloadFragments empty_payload{};
    const TransferMetadata metadata{0x66, transfer_time, Priority::Immediate};

    // Should fail due to anonymous node.
    {
        scheduler_.setNow(TimePoint{200ms});

        const auto maybe_error = session->send(metadata, empty_payload);
        EXPECT_THAT(maybe_error, Optional(VariantWith<ArgumentError>(_)));

        scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
        scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    }

    // Fix anonymous node
    {
        scheduler_.setNow(TimePoint{300ms});
        const auto send_time = now();

        EXPECT_THAT(transport->setLocalNodeId(13), Eq(cetl::nullopt));
        const auto maybe_error = session->send(metadata, empty_payload);
        EXPECT_THAT(maybe_error, Eq(cetl::nullopt));

        EXPECT_CALL(tx_socket_mock_, send(_, _, _, _)).WillOnce([&](auto deadline, auto endpoint, auto, auto) {
            EXPECT_THAT(now(), send_time + 10ms);
            EXPECT_THAT(deadline, transfer_time + timeout);
            EXPECT_THAT(endpoint.ip_address, 0xEF01001F);
            return true;
        });

        scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
        scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    }
}

TEST_F(TestUdpSvcTxSessions, make_response_session)
{
    auto transport = makeTransport({mr_}, NodeId{UDPARD_NODE_ID_MAX});

    auto maybe_session = transport->makeResponseTxSession({123});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IResponseTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IResponseTxSession>>(std::move(maybe_session));

    EXPECT_THAT(session->getParams().service_id, 123);

    EXPECT_THAT(session->run(now()), UbVariantWithoutValue());
}

TEST_F(TestUdpSvcTxSessions, make_response_fails_due_to_argument_error)
{
    auto transport = makeTransport({mr_}, NodeId{0});

    // Try invalid service id
    auto maybe_session = transport->makeResponseTxSession({UDPARD_SERVICE_ID_MAX + 1});
    EXPECT_THAT(maybe_session, VariantWith<AnyError>(VariantWith<ArgumentError>(_)));
}

TEST_F(TestUdpSvcTxSessions, make_response_fails_due_to_no_memory)
{
    StrictMock<MemoryResourceMock> mr_mock{};
    mr_mock.redirectExpectedCallsTo(mr_);

    // Emulate that there is no memory available for the message session.
    EXPECT_CALL(mr_mock, do_allocate(sizeof(udp::detail::SvcRequestTxSession), _)).WillOnce(Return(nullptr));

    auto transport = makeTransport({mr_mock}, NodeId{UDPARD_NODE_ID_MAX});

    auto maybe_session = transport->makeResponseTxSession({0x23});
    EXPECT_THAT(maybe_session, VariantWith<AnyError>(VariantWith<MemoryError>(_)));
}

TEST_F(TestUdpSvcTxSessions, send_response)
{
    auto transport = makeTransport({mr_}, NodeId{0x1F});

    auto maybe_session = transport->makeResponseTxSession({123});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IResponseTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IResponseTxSession>>(std::move(maybe_session));

    scheduler_.runNow(+10s);
    const auto send_time = now();
    const auto timeout   = 100ms;
    session->setSendTimeout(timeout);

    const PayloadFragments        empty_payload{};
    const ServiceTransferMetadata metadata{0x66, send_time, Priority::Fast, 0x0D};

    auto maybe_error = session->send(metadata, empty_payload);
    EXPECT_THAT(maybe_error, Eq(cetl::nullopt));

    EXPECT_CALL(tx_socket_mock_, send(_, _, _, _))
        .WillOnce([&](auto deadline, auto endpoint, auto dscp, auto fragments) {
            EXPECT_THAT(now(), send_time + 10ms);
            EXPECT_THAT(deadline, send_time + timeout);
            EXPECT_THAT(endpoint.ip_address, 0xEF01000D);
            EXPECT_THAT(endpoint.udp_port, 9382);
            EXPECT_THAT(dscp, 0x0);
            EXPECT_THAT(fragments, SizeIs(1));
            EXPECT_THAT(fragments[0], SizeIs(24 + 4));
            return true;
        });

    scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
}

TEST_F(TestUdpSvcTxSessions, send_response_with_argument_error)
{
    // Make initially anonymous node transport.
    //
    std::array<IMedia*, 1> media_array{&media_mock_};
    auto                   maybe_transport = udp::makeTransport({mr_}, mux_mock_, media_array, 2);
    ASSERT_THAT(maybe_transport, VariantWith<UniquePtr<IUdpTransport>>(NotNull()));
    auto transport = cetl::get<UniquePtr<IUdpTransport>>(std::move(maybe_transport));

    auto maybe_session = transport->makeResponseTxSession({123});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IResponseTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IResponseTxSession>>(std::move(maybe_session));

    const PayloadFragments  empty_payload{};
    ServiceTransferMetadata metadata{0x66, now(), Priority::Immediate, 13};

    // Should fail due to anonymous node.
    {
        scheduler_.setNow(TimePoint{100ms});

        const auto maybe_error = session->send(metadata, empty_payload);
        EXPECT_THAT(maybe_error, Optional(VariantWith<ArgumentError>(_)));

        scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
        scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    }

    // Fix anonymous node, but break remote node id.
    {
        scheduler_.setNow(TimePoint{200ms});

        EXPECT_THAT(transport->setLocalNodeId(31), Eq(cetl::nullopt));
        metadata.remote_node_id = UDPARD_NODE_ID_MAX + 1;
        const auto maybe_error  = session->send(metadata, empty_payload);
        EXPECT_THAT(maybe_error, Optional(VariantWith<ArgumentError>(_)));

        scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
        scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    }
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
