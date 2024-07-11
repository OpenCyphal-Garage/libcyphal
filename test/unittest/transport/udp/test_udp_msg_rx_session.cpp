/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "../../cetl_gtest_helpers.hpp"
#include "../../executor_mock.hpp"
#include "../../memory_resource_mock.hpp"
#include "../../tracking_memory_resource.hpp"
#include "../../verification_utilities.hpp"
#include "../../virtual_time_scheduler.hpp"
#include "media_mock.hpp"
#include "transient_error_handler_mock.hpp"
#include "tx_rx_sockets_mock.hpp"
#include "udp_gtest_helpers.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/msg_sessions.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/transport/udp/media.hpp>
#include <libcyphal/transport/udp/msg_rx_session.hpp>
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
#include <memory>
#include <utility>

namespace
{

using libcyphal::TimePoint;
using libcyphal::UniquePtr;
using namespace libcyphal::transport;       // NOLINT This our main concern here in the unit tests.
using namespace libcyphal::transport::udp;  // NOLINT This our main concern here in the unit tests.

using cetl::byte;
using libcyphal::verification_utilities::b;

using testing::_;
using testing::Eq;
using testing::Truly;
using testing::Invoke;
using testing::Return;
using testing::IsEmpty;
using testing::NotNull;
using testing::Optional;
using testing::StrictMock;
using testing::ElementsAre;
using testing::VariantWith;

// https://github.com/llvm/llvm-project/issues/53444
// NOLINTBEGIN(misc-unused-using-decls, misc-include-cleaner)
using std::literals::chrono_literals::operator""s;
using std::literals::chrono_literals::operator""ms;
// NOLINTEND(misc-unused-using-decls, misc-include-cleaner)

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class TestUdpMsgRxSession : public testing::Test
{
protected:
    void SetUp() override
    {
        EXPECT_CALL(media_mock_, makeTxSocket()).WillRepeatedly(Invoke([this]() {
            return libcyphal::detail::makeUniquePtr<TxSocketMock::ReferenceWrapper::Spec>(mr_, tx_socket_mock_);
        }));
        EXPECT_CALL(tx_socket_mock_, getMtu()).WillRepeatedly(Return(UDPARD_MTU_DEFAULT));

        EXPECT_CALL(media_mock_, makeRxSocket(_)).WillRepeatedly(Invoke([this](auto& endpoint) {
            rx_socket_mock_.setEndpoint(endpoint);
            return libcyphal::detail::makeUniquePtr<RxSocketMock::ReferenceWrapper::Spec>(mr_, rx_socket_mock_);
        }));
    }

    void TearDown() override
    {
        EXPECT_THAT(mr_.allocations, IsEmpty());
        EXPECT_THAT(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);

        EXPECT_THAT(payload_mr_.allocations, IsEmpty());
        EXPECT_THAT(payload_mr_.total_allocated_bytes, payload_mr_.total_deallocated_bytes);
    }

    TimePoint now() const
    {
        return scheduler_.now();
    }

    UniquePtr<IUdpTransport> makeTransport(const MemoryResourcesSpec&   mem_res_spec,
                                           const cetl::optional<NodeId> local_node_id = cetl::nullopt)
    {
        std::array<IMedia*, 1> media_array{&media_mock_};

        auto maybe_transport = udp::makeTransport(mem_res_spec, executor_mock_, media_array, 0);
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
    libcyphal::VirtualTimeScheduler     scheduler_{};
    TrackingMemoryResource              mr_;
    TrackingMemoryResource              payload_mr_;
    StrictMock<libcyphal::ExecutorMock> executor_mock_{};
    StrictMock<MediaMock>               media_mock_{};
    StrictMock<RxSocketMock>            rx_socket_mock_{"RxS1"};
    StrictMock<TxSocketMock>            tx_socket_mock_{"TxS1"};
    // NOLINTEND
};

// MARK: Tests:

TEST_F(TestUdpMsgRxSession, make_setTransferIdTimeout)
{
    auto transport = makeTransport({mr_});

    auto maybe_session = transport->makeMessageRxSession({42, 123});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageRxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageRxSession>>(std::move(maybe_session));

    EXPECT_THAT(session->getParams().extent_bytes, 42);
    EXPECT_THAT(session->getParams().subject_id, 123);

    session->setTransferIdTimeout(0s);
    session->setTransferIdTimeout(500ms);
}

TEST_F(TestUdpMsgRxSession, make_no_memory)
{
    StrictMock<MemoryResourceMock> mr_mock{};
    mr_mock.redirectExpectedCallsTo(mr_);

    auto transport = makeTransport({mr_mock});

    // Emulate that there is no memory available for the message session.
    EXPECT_CALL(mr_mock, do_allocate(sizeof(udp::detail::MessageRxSession), _)).WillOnce(Return(nullptr));

    auto maybe_session = transport->makeMessageRxSession({64, 0x23});
    EXPECT_THAT(maybe_session, VariantWith<AnyFailure>(VariantWith<MemoryError>(_)));
}

TEST_F(TestUdpMsgRxSession, make_fails_due_to_argument_error)
{
    auto transport = makeTransport({mr_});

    // Try invalid subject id
    auto maybe_session = transport->makeMessageRxSession({64, UDPARD_SUBJECT_ID_MAX + 1});
    EXPECT_THAT(maybe_session, VariantWith<AnyFailure>(VariantWith<ArgumentError>(_)));
}

TEST_F(TestUdpMsgRxSession, make_fails_due_to_rx_socket_error)
{
    using MediaReport = IUdpTransport::TransientErrorReport::MediaMakeRxSocket;

    auto transport = makeTransport({mr_});

    // Emulate that RX socket creation fails due to a memory error.
    {
        EXPECT_CALL(media_mock_, makeRxSocket(_)).WillOnce(Return(MemoryError{}));

        auto maybe_session = transport->makeMessageRxSession({64, 0x17B});
        EXPECT_THAT(maybe_session, VariantWith<AnyFailure>(VariantWith<MemoryError>(_)));
    }

    // Try again but with error handler.
    {
        EXPECT_CALL(media_mock_, makeRxSocket(_)).WillOnce(Return(nullptr));

        StrictMock<TransientErrorHandlerMock> handler_mock{};
        transport->setTransientErrorHandler(std::ref(handler_mock));
        EXPECT_CALL(handler_mock, invoke(VariantWith<MediaReport>(Truly([&](auto& report) {
                        EXPECT_THAT(report.failure, VariantWith<MemoryError>(_));
                        EXPECT_THAT(report.media_index, 0);
                        EXPECT_THAT(report.culprit, Ref(media_mock_));
                        return true;
                    }))))
            .WillOnce(Return(CapacityError{}));

        auto maybe_session = transport->makeMessageRxSession({64, 0x17B});
        EXPECT_THAT(maybe_session, VariantWith<AnyFailure>(VariantWith<CapacityError>(_)));
    }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_F(TestUdpMsgRxSession, run_and_receive)
{
    StrictMock<MemoryResourceMock>        payload_mr_mock{};
    StrictMock<TransientErrorHandlerMock> handler_mock{};

    auto transport = makeTransport({mr_, nullptr, nullptr, &payload_mr_mock}, NodeId{0x31});
    transport->setTransientErrorHandler(std::ref(handler_mock));

    auto maybe_session = transport->makeMessageRxSession({4, 0x23});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageRxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageRxSession>>(std::move(maybe_session));

    const auto params = session->getParams();
    EXPECT_THAT(params.extent_bytes, 4);
    EXPECT_THAT(params.subject_id, 0x23);

    const auto timeout = 200ms;
    session->setTransferIdTimeout(timeout);

    {
        SCOPED_TRACE("1-st iteration: one frame available @ 1s");

        scheduler_.setNow(TimePoint{1s});
        const auto rx_timestamp = now();

        const std::size_t payload_size = 2;
        const std::size_t frame_size   = UdpardFrame::SizeOfHeaderAndTxCrc + payload_size;

        EXPECT_CALL(payload_mr_mock, do_allocate(frame_size, alignof(std::max_align_t)))
            .WillOnce([this](std::size_t size_bytes, std::size_t alignment) -> void* {
                return payload_mr_.allocate(size_bytes, alignment);
            });
        EXPECT_CALL(rx_socket_mock_, receive()).WillOnce([&]() -> IRxSocket::ReceiveResult::Metadata {
            EXPECT_THAT(now(), rx_timestamp + 10ms);
            auto frame = UdpardFrame(0x13, UDPARD_NODE_ID_UNSET, 0x0D, payload_size, &payload_mr_mock, Priority::High);
            frame.payload()[0] = b('0');
            frame.payload()[1] = b('1');
            frame.setPortId(0x23, false /*is_service*/);
            std::uint32_t tx_crc = UdpardFrame::InitialTxCrc;
            return {rx_timestamp, std::move(frame).release(tx_crc)};
        });

        scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });

        const auto maybe_rx_transfer = session->receive();
        ASSERT_THAT(maybe_rx_transfer, Optional(_));
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        const auto& rx_transfer = maybe_rx_transfer.value();

        EXPECT_THAT(rx_transfer.metadata.timestamp, rx_timestamp);
        EXPECT_THAT(rx_transfer.metadata.transfer_id, 0x0D);
        EXPECT_THAT(rx_transfer.metadata.priority, Priority::High);
        EXPECT_THAT(rx_transfer.metadata.publisher_node_id, Optional(0x13));

        std::array<char, 2> buffer{};
        ASSERT_THAT(rx_transfer.payload.size(), buffer.size());
        EXPECT_THAT(rx_transfer.payload.copy(0, buffer.data(), buffer.size()), buffer.size());
        EXPECT_THAT(buffer, ElementsAre('0', '1'));

        EXPECT_CALL(payload_mr_mock, do_deallocate(_, frame_size, alignof(std::max_align_t)))
            .WillOnce([this](void* p, std::size_t size_bytes, std::size_t alignment) {
                payload_mr_.deallocate(p, size_bytes, alignment);
            });
    }
    {
        SCOPED_TRACE("2-nd iteration: invalid null frame available @ 2s");

        using UdpardReport = IUdpTransport::TransientErrorReport::UdpardRxMsgReceive;

        scheduler_.setNow(TimePoint{2s});
        const auto rx_timestamp = now();

        EXPECT_CALL(rx_socket_mock_, receive()).WillOnce([&]() -> IRxSocket::ReceiveResult::Metadata {
            return {rx_timestamp, {nullptr, libcyphal::PmrRawBytesDeleter{0, &payload_mr_mock}}};
        });
        EXPECT_CALL(handler_mock, invoke(VariantWith<UdpardReport>(Truly([&](auto& report) {
                        EXPECT_THAT(report.failure, VariantWith<ArgumentError>(_));
                        EXPECT_THAT(report.media_index, 0);
                        EXPECT_THAT(report.culprit.udp_ip_endpoint.ip_address, 0xEF000023);
                        return true;
                    }))))
            .WillOnce(Return(CapacityError{}));

        scheduler_.runNow(+10ms, [&] {
            EXPECT_THAT(transport->run(now()), UbVariantWith<AnyFailure>(VariantWith<CapacityError>(_)));
        });

        const auto maybe_rx_transfer = session->receive();
        EXPECT_THAT(maybe_rx_transfer, Eq(cetl::nullopt));
    }
    {
        SCOPED_TRACE("3-rd iteration: malformed frame available @ 3s - no error, just drop");

        scheduler_.setNow(TimePoint{3s});
        const auto rx_timestamp = now();

        const std::size_t payload_size = 0;
        const std::size_t frame_size   = UdpardFrame::SizeOfHeaderAndTxCrc + payload_size;

        EXPECT_CALL(payload_mr_mock, do_allocate(frame_size, alignof(std::max_align_t)))
            .WillOnce([this](std::size_t size_bytes, std::size_t alignment) -> void* {
                return payload_mr_.allocate(size_bytes, alignment);
            });
        EXPECT_CALL(rx_socket_mock_, receive()).WillOnce([&]() -> IRxSocket::ReceiveResult::Metadata {
            EXPECT_THAT(now(), rx_timestamp + 10ms);
            auto frame = UdpardFrame(0x13, UDPARD_NODE_ID_UNSET, 0x0D, payload_size, &payload_mr_mock, Priority::High);
            frame.setPortId(0x23, true /*is_service*/);  // This makes it invalid (b/c we expect messages).
            std::uint32_t tx_crc = UdpardFrame::InitialTxCrc;
            return {rx_timestamp, std::move(frame).release(tx_crc)};
        });
        EXPECT_CALL(payload_mr_mock, do_deallocate(_, frame_size, alignof(std::max_align_t)))
            .WillOnce([this](void* p, std::size_t size_bytes, std::size_t alignment) {
                payload_mr_.deallocate(p, size_bytes, alignment);
            });

        scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });

        const auto maybe_rx_transfer = session->receive();
        EXPECT_THAT(maybe_rx_transfer, Eq(cetl::nullopt));
    }
}

TEST_F(TestUdpMsgRxSession, run_and_receive_one_anonymous_frame)
{
    StrictMock<MemoryResourceMock> payload_mr_mock{};

    auto transport = makeTransport({mr_, nullptr, nullptr, &payload_mr_mock});

    auto maybe_session = transport->makeMessageRxSession({4, 0x23});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageRxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageRxSession>>(std::move(maybe_session));

    const auto params = session->getParams();
    EXPECT_THAT(params.extent_bytes, 4);
    EXPECT_THAT(params.subject_id, 0x23);

    const auto timeout = 200ms;
    session->setTransferIdTimeout(timeout);

    scheduler_.setNow(TimePoint{1s});
    const auto rx_timestamp = now();

    const std::size_t payload_size = 2;
    const std::size_t frame_size   = UdpardFrame::SizeOfHeaderAndTxCrc + payload_size;

    EXPECT_CALL(payload_mr_mock, do_allocate(frame_size, alignof(std::max_align_t)))
        .WillOnce([this](std::size_t size_bytes, std::size_t alignment) -> void* {
            return payload_mr_.allocate(size_bytes, alignment);
        });
    EXPECT_CALL(rx_socket_mock_, receive()).WillOnce([&]() -> IRxSocket::ReceiveResult::Metadata {
        EXPECT_THAT(now(), rx_timestamp + 10ms);
        auto frame = UdpardFrame(UDPARD_NODE_ID_UNSET,
                                 UDPARD_NODE_ID_UNSET,
                                 0x0D,
                                 payload_size,
                                 &payload_mr_mock,
                                 Priority::Low);

        frame.payload()[0] = b('0');
        frame.payload()[1] = b('1');
        frame.setPortId(0x23, false /*is_service*/);
        std::uint32_t tx_crc = UdpardFrame::InitialTxCrc;
        return {rx_timestamp, std::move(frame).release(tx_crc)};
    });

    scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });

    const auto maybe_rx_transfer = session->receive();
    ASSERT_THAT(maybe_rx_transfer, Optional(_));
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    const auto& rx_transfer = maybe_rx_transfer.value();

    EXPECT_THAT(rx_transfer.metadata.timestamp, rx_timestamp);
    EXPECT_THAT(rx_transfer.metadata.transfer_id, 0x0D);
    EXPECT_THAT(rx_transfer.metadata.priority, Priority::Low);
    EXPECT_THAT(rx_transfer.metadata.publisher_node_id, Eq(cetl::nullopt));

    std::array<char, 2> buffer{};
    ASSERT_THAT(rx_transfer.payload.size(), buffer.size());
    EXPECT_THAT(rx_transfer.payload.copy(0, buffer.data(), buffer.size()), buffer.size());
    EXPECT_THAT(buffer, ElementsAre('0', '1'));

    EXPECT_CALL(payload_mr_mock, do_deallocate(_, frame_size, alignof(std::max_align_t)))
        .WillOnce([this](void* p, std::size_t size_bytes, std::size_t alignment) {
            payload_mr_.deallocate(p, size_bytes, alignment);
        });
}

TEST_F(TestUdpMsgRxSession, unsubscribe_and_run)
{
    auto transport = makeTransport({mr_});

    auto maybe_session = transport->makeMessageRxSession({4, 0x23});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageRxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageRxSession>>(std::move(maybe_session));

    EXPECT_CALL(rx_socket_mock_, receive()).WillOnce([&]() { return cetl::nullopt; });
    scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });

    session.reset();

    scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    scheduler_.runNow(+10ms, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
