/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "../../cetl_gtest_helpers.hpp"
#include "../../memory_resource_mock.hpp"
#include "../../tracking_memory_resource.hpp"
#include "../../verification_utilities.hpp"
#include "../../virtual_time_scheduler.hpp"
#include "../multiplexer_mock.hpp"
#include "media_mock.hpp"
#include "transient_error_handler_mock.hpp"
#include "tx_rx_sockets_mock.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/msg_sessions.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/transport/udp/media.hpp>
#include <libcyphal/transport/udp/msg_tx_session.hpp>
#include <libcyphal/transport/udp/tx_rx_sockets.hpp>
#include <libcyphal/transport/udp/udp_transport.hpp>
#include <libcyphal/transport/udp/udp_transport_impl.hpp>
#include <libcyphal/types.hpp>
#include <udpard.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>

namespace
{

using libcyphal::TimePoint;
using libcyphal::UniquePtr;
using namespace libcyphal::transport;       // NOLINT This our main concern here in the unit tests.
using namespace libcyphal::transport::udp;  // NOLINT This our main concern here in the unit tests.

using cetl::byte;
using libcyphal::verification_utilities::b;
using libcyphal::verification_utilities::makeIotaArray;
using libcyphal::verification_utilities::makeSpansFrom;

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
using std::literals::chrono_literals::operator""us;
// NOLINTEND(misc-unused-using-decls, misc-include-cleaner)

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class TestUdpMsgTxSession : public testing::Test
{
protected:
    void SetUp() override
    {
        EXPECT_CALL(media_mock_, makeTxSocket()).WillRepeatedly(Invoke([this]() {
            return libcyphal::detail::makeUniquePtr<TxSocketMock::ReferenceWrapper::Spec>(mr_, tx_socket_mock_);
        }));
        EXPECT_CALL(tx_socket_mock_, getMtu()).WillRepeatedly(Return(UDPARD_MTU_DEFAULT));
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

    UniquePtr<IUdpTransport> makeTransport(const MemoryResourcesSpec& mem_res_spec)
    {
        std::array<IMedia*, 1> media_array{&media_mock_};

        auto maybe_transport = udp::makeTransport(mem_res_spec, mux_mock_, media_array, 16);
        EXPECT_THAT(maybe_transport, VariantWith<UniquePtr<IUdpTransport>>(NotNull()));
        return cetl::get<UniquePtr<IUdpTransport>>(std::move(maybe_transport));
    }

    // MARK: Data members:

    // NOLINTBEGIN
    libcyphal::VirtualTimeScheduler scheduler_{};
    TrackingMemoryResource          mr_;
    StrictMock<MultiplexerMock>     mux_mock_{};
    StrictMock<MediaMock>           media_mock_{};
    StrictMock<TxSocketMock>        tx_socket_mock_{"S1"};
    // NOLINTEND
};

// MARK: Tests:

TEST_F(TestUdpMsgTxSession, make)
{
    auto transport = makeTransport({mr_});

    auto maybe_session = transport->makeMessageTxSession({123});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));

    EXPECT_THAT(session->getParams().subject_id, 123);
}

TEST_F(TestUdpMsgTxSession, make_no_memory)
{
    StrictMock<MemoryResourceMock> mr_mock{};
    mr_mock.redirectExpectedCallsTo(mr_);

    auto transport = makeTransport({mr_mock});

    // Emulate that there is no memory available for the message session.
    EXPECT_CALL(mr_mock, do_allocate(sizeof(udp::detail::MessageTxSession), _)).WillOnce(Return(nullptr));

    auto maybe_session = transport->makeMessageTxSession({0x23});
    EXPECT_THAT(maybe_session, VariantWith<AnyFailure>(VariantWith<MemoryError>(_)));
}

TEST_F(TestUdpMsgTxSession, make_fails_due_to_argument_error)
{
    auto transport = makeTransport({mr_});

    // Try invalid subject id
    auto maybe_session = transport->makeMessageTxSession({UDPARD_SUBJECT_ID_MAX + 1});
    EXPECT_THAT(maybe_session, VariantWith<AnyFailure>(VariantWith<ArgumentError>(_)));
}

TEST_F(TestUdpMsgTxSession, make_fails_due_to_media_socket)
{
    using MakeSocketReport = IUdpTransport::TransientErrorReport::MediaMakeTxSocket;

    auto transport = makeTransport({mr_});

    // 1. Transport will fail to make msg TX session b/c media fails to create a TX socket.
    {
        EXPECT_CALL(media_mock_, makeTxSocket()).WillOnce(Return(MemoryError{}));

        auto maybe_tx_session = transport->makeMessageTxSession({123});
        EXPECT_THAT(maybe_tx_session, VariantWith<AnyFailure>(VariantWith<MemoryError>(_)));
    }

    // 2. Transport will succeed to make TX session despite the media fails to create a TX socket.
    //    This is b/c transient error handler will be set and will handle the error.
    {
        EXPECT_CALL(media_mock_, makeTxSocket()).WillOnce(Return(MemoryError{}));

        StrictMock<TransientErrorHandlerMock> handler_mock{};
        transport->setTransientErrorHandler(std::ref(handler_mock));
        EXPECT_CALL(handler_mock, invoke(VariantWith<MakeSocketReport>(Truly([&](auto& report) {
                        EXPECT_THAT(report.failure, VariantWith<MemoryError>(_));
                        EXPECT_THAT(report.media_index, 0);
                        EXPECT_THAT(report.culprit, Ref(media_mock_));
                        return true;
                    }))))
            .WillOnce(Return(cetl::nullopt));

        auto maybe_tx_session = transport->makeMessageTxSession({123});
        ASSERT_THAT(maybe_tx_session, VariantWith<UniquePtr<IMessageTxSession>>(NotNull()));

        auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_tx_session));
        EXPECT_THAT(session->getParams().subject_id, 123);
        EXPECT_THAT(transport->getProtocolParams().mtu_bytes, ITxSocket::DefaultMtu);
    }
}

TEST_F(TestUdpMsgTxSession, send_empty_payload_and_no_transport_run)
{
    StrictMock<MemoryResourceMock> fragment_mr_mock{};
    fragment_mr_mock.redirectExpectedCallsTo(mr_);

    auto transport = makeTransport({mr_, nullptr, &fragment_mr_mock});

    auto maybe_session = transport->makeMessageTxSession({123});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));

    const PayloadFragments empty_payload{};
    const TransferMetadata metadata{0x1AF52, {}, Priority::Low};

    // TX item for our payload to send is expected to be de/allocated on the *fragment* memory resource.
    //
    EXPECT_CALL(fragment_mr_mock, do_allocate(_, _))
        .WillOnce([&](std::size_t size_bytes, std::size_t alignment) -> void* {
            return mr_.allocate(size_bytes, alignment);
        });
    EXPECT_CALL(fragment_mr_mock, do_deallocate(_, _, _))
        .WillOnce(
            [&](void* p, std::size_t size_bytes, std::size_t alignment) { mr_.deallocate(p, size_bytes, alignment); });

    auto failure = session->send(metadata, empty_payload);
    EXPECT_THAT(failure, Eq(cetl::nullopt));

    scheduler_.runNow(+10ms, [&] { session->run(scheduler_.now()); });

    // Payload still inside udpard TX queue (b/c there was no `transport->run` call deliberately),
    // but there will be no memory leak b/c we expect that it should be deallocated when the transport is destroyed.
    // See `EXPECT_THAT(mr_.allocations, IsEmpty());` at the `TearDown` method.
}

TEST_F(TestUdpMsgTxSession, send_empty_payload)
{
    auto transport = makeTransport({mr_});

    auto maybe_session = transport->makeMessageTxSession({0x7B});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));

    scheduler_.runNow(+10s);
    const auto send_time = now();
    const auto timeout   = 1s;

    const PayloadFragments empty_payload{};
    const TransferMetadata metadata{0x3AF52, send_time, Priority::Low};

    auto failure = session->send(metadata, empty_payload);
    EXPECT_THAT(failure, Eq(cetl::nullopt));

    EXPECT_CALL(tx_socket_mock_, send(_, _, _, _))
        .WillOnce([&](auto deadline, auto endpoint, auto dscp, auto fragments) {
            EXPECT_THAT(now(), send_time + 10ms);
            EXPECT_THAT(deadline, send_time + timeout);
            EXPECT_THAT(endpoint.ip_address, 0xEF00007B);
            EXPECT_THAT(endpoint.udp_port, 9382);
            EXPECT_THAT(dscp, 0x0);
            EXPECT_THAT(fragments, SizeIs(1));
            EXPECT_THAT(fragments[0], SizeIs(24 + 4));
            return ITxSocket::SendResult::Success{true /*is_accepted*/};
        });

    scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
}

TEST_F(TestUdpMsgTxSession, send_empty_expired_payload)
{
    auto transport = makeTransport({mr_});

    auto maybe_session = transport->makeMessageTxSession({123});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));

    scheduler_.runNow(+10s);
    const auto send_time = now();
    const auto timeout   = 1s;

    const PayloadFragments empty_payload{};
    const TransferMetadata metadata{0x11, send_time, Priority::Low};

    auto failure = session->send(metadata, empty_payload);
    EXPECT_THAT(failure, Eq(cetl::nullopt));

    // Emulate run calls just on the very edge of the default 1s timeout (exactly at the deadline).
    // Payload should NOT be sent but dropped instead.
    //
    scheduler_.runNow(+timeout + 1us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    scheduler_.runNow(+1us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
}

TEST_F(TestUdpMsgTxSession, send_single_frame_payload_with_500ms_timeout)
{
    auto transport = makeTransport({mr_});

    auto maybe_session = transport->makeMessageTxSession({0x17});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));

    const auto timeout = 500ms;
    session->setSendTimeout(timeout);

    scheduler_.runNow(+10s);
    const auto send_time = now();

    const auto             payload = makeIotaArray<UDPARD_MTU_DEFAULT_MAX_SINGLE_FRAME>(b('1'));
    const TransferMetadata metadata{0x03, send_time, Priority::High};

    auto failure = session->send(metadata, makeSpansFrom(payload));
    EXPECT_THAT(failure, Eq(cetl::nullopt));

    // Emulate run calls just on the very edge of the 500ms deadline.
    // Payload should be sent successfully.
    //
    EXPECT_CALL(tx_socket_mock_, send(_, _, _, _)).WillOnce([&](auto, auto endpoint, auto dscp, auto fragments) {
        EXPECT_THAT(now(), send_time + timeout - 1us);
        EXPECT_THAT(endpoint.ip_address, 0xEF000017);
        EXPECT_THAT(endpoint.udp_port, 9382);
        EXPECT_THAT(dscp, 0x0);
        EXPECT_THAT(fragments, SizeIs(1));
        EXPECT_THAT(fragments[0], SizeIs(24 + UDPARD_MTU_DEFAULT_MAX_SINGLE_FRAME + 4));
        EXPECT_THAT(fragments[0][24 + 0], b('1'));
        EXPECT_THAT(fragments[0][24 + 1], b('2'));
        EXPECT_THAT(fragments[0][24 + UDPARD_MTU_DEFAULT_MAX_SINGLE_FRAME - 1],
                    b(static_cast<std::uint8_t>('1' + UDPARD_MTU_DEFAULT_MAX_SINGLE_FRAME - 1)));
        return ITxSocket::SendResult::Success{true /*is_accepted*/};
    });
    //
    scheduler_.runNow(timeout - 1us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    scheduler_.runNow(+0us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
}

TEST_F(TestUdpMsgTxSession, send_when_no_memory_for_contiguous_payload)
{
    StrictMock<MemoryResourceMock> mr_mock{};
    mr_mock.redirectExpectedCallsTo(mr_);

    auto transport = makeTransport({mr_mock});

    // Emulate that there is no memory available for the expected contiguous payload.
    const auto payload1 = makeIotaArray<1>(b('0'));
    const auto payload2 = makeIotaArray<2>(b('1'));
    EXPECT_CALL(mr_mock, do_allocate(sizeof(payload1) + sizeof(payload2), _)).WillOnce(Return(nullptr));

    auto maybe_session = transport->makeMessageTxSession({17});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));

    scheduler_.runNow(+10s);
    const auto send_time = now();

    const TransferMetadata metadata{0x03, send_time, Priority::Optional};

    auto failure = session->send(metadata, makeSpansFrom(payload1, payload2));
    EXPECT_THAT(failure, Optional(VariantWith<MemoryError>(_)));

    scheduler_.runNow(+10ms, [&] { transport->run(scheduler_.now()); });
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
