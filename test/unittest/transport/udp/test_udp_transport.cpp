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

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/rtti.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/msg_sessions.hpp>
#include <libcyphal/transport/svc_sessions.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/transport/udp/media.hpp>
#include <libcyphal/transport/udp/tx_rx_sockets.hpp>
#include <libcyphal/transport/udp/udp_transport.hpp>
#include <libcyphal/transport/udp/udp_transport_impl.hpp>
#include <libcyphal/types.hpp>
#include <udpard.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <utility>

namespace
{

using libcyphal::TimePoint;
using libcyphal::UniquePtr;
using namespace libcyphal::transport;       // NOLINT This our main concern here in the unit tests.
using namespace libcyphal::transport::udp;  // NOLINT This our main concern here in the unit tests.

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
using testing::Optional;
using testing::InSequence;
using testing::StrictMock;
using testing::VariantWith;

// https://github.com/llvm/llvm-project/issues/53444
// NOLINTBEGIN(misc-unused-using-decls, misc-include-cleaner)
using std::literals::chrono_literals::operator""s;
using std::literals::chrono_literals::operator""us;
// NOLINTEND(misc-unused-using-decls, misc-include-cleaner)

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)

class MyPlatformError final : public libcyphal::transport::IPlatformError
{
public:
    explicit MyPlatformError(std::uint32_t code)
        : code_{code}
    {
    }
    virtual ~MyPlatformError() noexcept                    = default;
    MyPlatformError(const MyPlatformError&)                = default;
    MyPlatformError(MyPlatformError&&) noexcept            = default;
    MyPlatformError& operator=(const MyPlatformError&)     = default;
    MyPlatformError& operator=(MyPlatformError&&) noexcept = default;

    // MARK: IPlatformError

    std::uint32_t code() const noexcept override
    {
        return code_;
    }

private:
    std::uint32_t code_;

};  // MyPlatformError

class TestUpdTransport : public testing::Test
{
protected:
    void SetUp() override
    {
        EXPECT_CALL(media_mock_, makeTxSocket()).WillRepeatedly(Invoke([this]() {
            return libcyphal::detail::makeUniquePtr<TxSocketMock::ReferenceWrapper::Spec>(mr_, tx_socket_mock_);
        }));
        EXPECT_CALL(media_mock_, makeRxSocket(_)).WillRepeatedly(Invoke([this](auto& endpoint) {
            rx_socket_mock_.setEndpoint(endpoint);
            return libcyphal::detail::makeUniquePtr<RxSocketMock::ReferenceWrapper::Spec>(mr_, rx_socket_mock_);
        }));

        EXPECT_CALL(tx_socket_mock_, getMtu()).WillRepeatedly(Invoke(&tx_socket_mock_, &TxSocketMock::getBaseMtu));
        EXPECT_CALL(rx_socket_mock_, receive()).WillRepeatedly(Invoke([]() { return cetl::nullopt; }));
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

    UniquePtr<IUdpTransport> makeTransport(const MemoryResourcesSpec& mem_res_spec,
                                           IMedia*                    extra_media = nullptr,
                                           const std::size_t          tx_capacity = 16)
    {
        std::array<IMedia*, 2> media_array{&media_mock_, extra_media};

        auto maybe_transport = udp::makeTransport(mem_res_spec, executor_mock_, media_array, tx_capacity);
        EXPECT_THAT(maybe_transport, VariantWith<UniquePtr<IUdpTransport>>(NotNull()));
        return cetl::get<UniquePtr<IUdpTransport>>(std::move(maybe_transport));
    }

    // MARK: Data members:

    // NOLINTBEGIN
    libcyphal::VirtualTimeScheduler     scheduler_{};
    TrackingMemoryResource              mr_;
    StrictMock<libcyphal::ExecutorMock> executor_mock_{};
    StrictMock<MediaMock>               media_mock_{};
    StrictMock<RxSocketMock>            rx_socket_mock_{"RxS1"};
    StrictMock<TxSocketMock>            tx_socket_mock_{"TxS1"};
    // NOLINTEND
};

// MARK: Tests:

TEST_F(TestUpdTransport, makeTransport_no_memory_at_all)
{
    StrictMock<MemoryResourceMock> mr_mock{};
    mr_mock.redirectExpectedCallsTo(mr_);

    // Emulate that there is no memory at all (even for initial array of media).
    EXPECT_CALL(mr_mock, do_allocate(_, _)).WillRepeatedly(Return(nullptr));
#if (__cplusplus < CETL_CPP_STANDARD_17)
    EXPECT_CALL(mr_mock, do_reallocate(nullptr, 0, _, _)).WillRepeatedly(Return(nullptr));
#endif

    std::array<IMedia*, 1> media_array{&media_mock_};
    auto                   maybe_transport = udp::makeTransport({mr_mock}, executor_mock_, media_array, 0);
    EXPECT_THAT(maybe_transport, VariantWith<FactoryFailure>(VariantWith<MemoryError>(_)));
}

TEST_F(TestUpdTransport, makeTransport_no_memory_for_impl)
{
    StrictMock<MemoryResourceMock> mr_mock{};
    mr_mock.redirectExpectedCallsTo(mr_);

    // Emulate that there is no memory available for the transport.
    EXPECT_CALL(mr_mock, do_allocate(sizeof(udp::detail::TransportImpl), _)).WillOnce(Return(nullptr));

    std::array<IMedia*, 1> media_array{&media_mock_};
    auto                   maybe_transport = udp::makeTransport({mr_mock}, executor_mock_, media_array, 0);
    EXPECT_THAT(maybe_transport, VariantWith<FactoryFailure>(VariantWith<MemoryError>(_)));
}

TEST_F(TestUpdTransport, makeTransport_too_many_media)
{
    std::array<IMedia*, UDPARD_NETWORK_INTERFACE_COUNT_MAX + 1> media_array{};
    std::fill(media_array.begin(), media_array.end(), &media_mock_);

    auto maybe_transport = udp::makeTransport({mr_}, executor_mock_, media_array, 0);
    EXPECT_THAT(maybe_transport, VariantWith<FactoryFailure>(VariantWith<ArgumentError>(_)));
}

TEST_F(TestUpdTransport, makeTransport_getLocalNodeId)
{
    // Anonymous node
    {
        std::array<IMedia*, 1> media_array{&media_mock_};
        auto                   maybe_transport = udp::makeTransport({mr_}, executor_mock_, media_array, 0);
        ASSERT_THAT(maybe_transport, VariantWith<UniquePtr<IUdpTransport>>(NotNull()));

        auto transport = cetl::get<UniquePtr<IUdpTransport>>(std::move(maybe_transport));
        EXPECT_THAT(transport->getLocalNodeId(), Eq(cetl::nullopt));
    }

    // Node with ID
    {
        std::array<IMedia*, 1> media_array{&media_mock_};
        auto                   maybe_transport = udp::makeTransport({mr_}, executor_mock_, media_array, 0);
        ASSERT_THAT(maybe_transport, VariantWith<UniquePtr<IUdpTransport>>(NotNull()));

        auto transport = cetl::get<UniquePtr<IUdpTransport>>(std::move(maybe_transport));
        transport->setLocalNodeId(42);

        EXPECT_THAT(transport->getLocalNodeId(), Optional(42));
    }

    // Two media interfaces
    {
        StrictMock<MediaMock> media_mock2;

        std::array<IMedia*, 3> media_array{&media_mock_, nullptr, &media_mock2};
        auto                   maybe_transport = udp::makeTransport({mr_}, executor_mock_, media_array, 0);
        EXPECT_THAT(maybe_transport, VariantWith<UniquePtr<IUdpTransport>>(NotNull()));
    }

    // All 3 maximum number of media interfaces
    {
        StrictMock<MediaMock> media_mock2{};
        StrictMock<MediaMock> media_mock3{};

        std::array<IMedia*, 3> media_array{&media_mock_, &media_mock2, &media_mock3};
        auto                   maybe_transport = udp::makeTransport({mr_}, executor_mock_, media_array, 0);
        EXPECT_THAT(maybe_transport, VariantWith<UniquePtr<IUdpTransport>>(NotNull()));
    }
}

TEST_F(TestUpdTransport, setLocalNodeId)
{
    auto transport = makeTransport({mr_});

    EXPECT_THAT(transport->setLocalNodeId(UDPARD_NODE_ID_MAX + 1), Optional(testing::A<ArgumentError>()));
    EXPECT_THAT(transport->getLocalNodeId(), Eq(cetl::nullopt));

    EXPECT_THAT(transport->run(now()), UbVariantWithoutValue());

    EXPECT_THAT(transport->setLocalNodeId(UDPARD_NODE_ID_MAX), Eq(cetl::nullopt));
    EXPECT_THAT(transport->getLocalNodeId(), Optional(UDPARD_NODE_ID_MAX));

    EXPECT_THAT(transport->run(now()), UbVariantWithoutValue());
    EXPECT_THAT(rx_socket_mock_.getEndpoint().ip_address, 0);
    EXPECT_THAT(rx_socket_mock_.getEndpoint().udp_port, 0);

    EXPECT_THAT(transport->setLocalNodeId(UDPARD_NODE_ID_MAX), Eq(cetl::nullopt));
    EXPECT_THAT(transport->getLocalNodeId(), Optional(UDPARD_NODE_ID_MAX));

    EXPECT_THAT(transport->run(now()), UbVariantWithoutValue());

    EXPECT_THAT(transport->setLocalNodeId(0), Optional(testing::A<ArgumentError>()));
    EXPECT_THAT(transport->getLocalNodeId(), Optional(UDPARD_NODE_ID_MAX));

    EXPECT_THAT(transport->run(now()), UbVariantWithoutValue());
}

TEST_F(TestUpdTransport, makeTransport_with_invalid_arguments)
{
    // No media
    const auto maybe_transport = udp::makeTransport({mr_}, executor_mock_, {}, 0);
    EXPECT_THAT(maybe_transport, VariantWith<FactoryFailure>(VariantWith<ArgumentError>(_)));
}

TEST_F(TestUpdTransport, getProtocolParams)
{
    StrictMock<MediaMock> media_mock2{};

    std::array<IMedia*, 2> media_array{&media_mock_, &media_mock2};
    auto transport = cetl::get<UniquePtr<IUdpTransport>>(udp::makeTransport({mr_}, executor_mock_, media_array, 0));

    const auto params = transport->getProtocolParams();
    EXPECT_THAT(params.transfer_id_modulo, std::numeric_limits<TransferId>::max());
    EXPECT_THAT(params.max_nodes, UDPARD_NODE_ID_MAX + 1);
    EXPECT_THAT(params.mtu_bytes, UDPARD_MTU_DEFAULT);

    StrictMock<TxSocketMock> tx_socket_mock2{"S2"};
    EXPECT_CALL(media_mock2, makeTxSocket()).WillRepeatedly(Invoke([&]() {
        return libcyphal::detail::makeUniquePtr<TxSocketMock::ReferenceWrapper::Spec>(mr_, tx_socket_mock2);
    }));
    EXPECT_CALL(tx_socket_mock2, getMtu()).WillRepeatedly(Return(ITxSocket::DefaultMtu));

    auto maybe_tx_session = transport->makeMessageTxSession({123});
    ASSERT_THAT(maybe_tx_session, VariantWith<UniquePtr<IMessageTxSession>>(NotNull()));
    EXPECT_THAT(transport->getProtocolParams().mtu_bytes, UDPARD_MTU_DEFAULT);

    EXPECT_CALL(tx_socket_mock_, getMtu()).WillRepeatedly(Return(UDPARD_MTU_DEFAULT));
    EXPECT_CALL(tx_socket_mock2, getMtu()).WillRepeatedly(Return(UDPARD_MTU_DEFAULT - 256));
    EXPECT_THAT(transport->getProtocolParams().mtu_bytes, UDPARD_MTU_DEFAULT - 256);

    // Manipulate MTU values on fly
    {
        EXPECT_CALL(tx_socket_mock2, getMtu()).WillRepeatedly(Return(UDPARD_MTU_DEFAULT));
        EXPECT_THAT(transport->getProtocolParams().mtu_bytes, UDPARD_MTU_DEFAULT);

        EXPECT_CALL(tx_socket_mock_, getMtu()).WillRepeatedly(Return(UDPARD_MTU_DEFAULT - 256));
        EXPECT_THAT(transport->getProtocolParams().mtu_bytes, UDPARD_MTU_DEFAULT - 256);

        EXPECT_CALL(tx_socket_mock2, getMtu()).WillRepeatedly(Return(UDPARD_MTU_DEFAULT - 256));
        EXPECT_THAT(transport->getProtocolParams().mtu_bytes, UDPARD_MTU_DEFAULT - 256);
    }
}

TEST_F(TestUpdTransport, makeMessageRxSession)
{
    auto transport = makeTransport({mr_});

    auto maybe_rx_session = transport->makeMessageRxSession({42, 123});
    ASSERT_THAT(maybe_rx_session, VariantWith<UniquePtr<IMessageRxSession>>(NotNull()));

    auto session = cetl::get<UniquePtr<IMessageRxSession>>(std::move(maybe_rx_session));
    EXPECT_THAT(session->getParams().extent_bytes, 42);
    EXPECT_THAT(session->getParams().subject_id, 123);
}

TEST_F(TestUpdTransport, makeMessageRxSession_invalid_subject_id)
{
    auto transport = makeTransport({mr_});

    auto maybe_rx_session = transport->makeMessageRxSession({0, UDPARD_SUBJECT_ID_MAX + 1});
    EXPECT_THAT(maybe_rx_session, VariantWith<AnyFailure>(VariantWith<ArgumentError>(_)));
}

TEST_F(TestUpdTransport, makeMessageRxSession_invalid_resubscription)
{
    auto transport = makeTransport({mr_});

    const PortId test_subject_id = 111;

    auto maybe_rx_session1 = transport->makeMessageRxSession({0, test_subject_id});
    ASSERT_THAT(maybe_rx_session1, VariantWith<UniquePtr<IMessageRxSession>>(NotNull()));

    auto maybe_rx_session2 = transport->makeMessageRxSession({0, test_subject_id});
    EXPECT_THAT(maybe_rx_session2, VariantWith<AnyFailure>(VariantWith<AlreadyExistsError>(_)));

    // Now release the first session and try to subscribe again - should succeed.
    {
        auto rx_session1 = cetl::get<UniquePtr<IMessageRxSession>>(std::move(maybe_rx_session1));
        rx_session1.reset();

        maybe_rx_session2 = transport->makeMessageRxSession({0, test_subject_id});
        ASSERT_THAT(maybe_rx_session2, VariantWith<UniquePtr<IMessageRxSession>>(NotNull()));
    }
}

TEST_F(TestUpdTransport, makeRequestRxSession_invalid_resubscription)
{
    auto transport = makeTransport({mr_});

    const PortId test_subject_id = 111;

    auto maybe_rx_session1 = transport->makeRequestRxSession({0, test_subject_id});
    ASSERT_THAT(maybe_rx_session1, VariantWith<UniquePtr<IRequestRxSession>>(NotNull()));

    auto maybe_rx_session2 = transport->makeRequestRxSession({0, test_subject_id});
    EXPECT_THAT(maybe_rx_session2, VariantWith<AnyFailure>(VariantWith<AlreadyExistsError>(_)));

    // Now release the first session and try to subscribe again - should succeed.
    {
        auto rx_session1 = cetl::get<UniquePtr<IRequestRxSession>>(std::move(maybe_rx_session1));
        rx_session1.reset();

        maybe_rx_session2 = transport->makeRequestRxSession({0, test_subject_id});
        ASSERT_THAT(maybe_rx_session2, VariantWith<UniquePtr<IRequestRxSession>>(NotNull()));
    }
}

TEST_F(TestUpdTransport, makeResponseRxSession_invalid_resubscription)
{
    auto transport = makeTransport({mr_});

    const PortId test_subject_id = 111;

    auto maybe_rx_session1 = transport->makeResponseRxSession({0, test_subject_id, 0x31});
    ASSERT_THAT(maybe_rx_session1, VariantWith<UniquePtr<IResponseRxSession>>(NotNull()));

    auto maybe_rx_session2 = transport->makeResponseRxSession({0, test_subject_id, 0x31});
    EXPECT_THAT(maybe_rx_session2, VariantWith<AnyFailure>(VariantWith<AlreadyExistsError>(_)));

    // Now release the first session and try to subscribe again - should succeed.
    {
        auto rx_session1 = cetl::get<UniquePtr<IResponseRxSession>>(std::move(maybe_rx_session1));
        rx_session1.reset();

        maybe_rx_session2 = transport->makeResponseRxSession({0, test_subject_id, 0x31});
        ASSERT_THAT(maybe_rx_session2, VariantWith<UniquePtr<IResponseRxSession>>(NotNull()));
    }
}

TEST_F(TestUpdTransport, makeXxxRxSession_all_with_same_port_id)
{
    auto transport = makeTransport({mr_});

    const PortId test_port_id = 111;

    auto maybe_svc_res_rx_session1 = transport->makeResponseRxSession({0, test_port_id, 0x31});
    ASSERT_THAT(maybe_svc_res_rx_session1, VariantWith<UniquePtr<IResponseRxSession>>(NotNull()));

    auto maybe_svc_req_rx_session1 = transport->makeRequestRxSession({0, test_port_id});
    ASSERT_THAT(maybe_svc_req_rx_session1, VariantWith<UniquePtr<IRequestRxSession>>(NotNull()));

    auto maybe_msg_rx_session = transport->makeMessageRxSession({42, test_port_id});
    ASSERT_THAT(maybe_msg_rx_session, VariantWith<UniquePtr<IMessageRxSession>>(NotNull()));
}

TEST_F(TestUpdTransport, makeMessageTxSession)
{
    auto transport = makeTransport({mr_});

    auto maybe_tx_session = transport->makeMessageTxSession({123});
    ASSERT_THAT(maybe_tx_session, VariantWith<UniquePtr<IMessageTxSession>>(NotNull()));

    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_tx_session));
    EXPECT_THAT(session->getParams().subject_id, 123);
}

TEST_F(TestUpdTransport, sending_multiframe_payload_should_fail_for_anonymous)
{
    auto transport = makeTransport({mr_});

    auto maybe_session = transport->makeMessageTxSession({7});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));

    scheduler_.runNow(+10s);
    const auto send_time = now();

    const auto             payload = makeIotaArray<UDPARD_MTU_DEFAULT_MAX_SINGLE_FRAME + 1>(b('0'));
    const TransferMetadata metadata{0x13, send_time, Priority::Nominal};

    auto failure = session->send(metadata, makeSpansFrom(payload));
    EXPECT_THAT(failure, Optional(VariantWith<AnonymousError>(_)));

    scheduler_.runNow(+10us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
}

TEST_F(TestUpdTransport, sending_multiframe_payload_for_non_anonymous)
{
    auto transport = makeTransport({mr_});
    EXPECT_THAT(transport->setLocalNodeId(0x45), Eq(cetl::nullopt));

    auto maybe_session = transport->makeMessageTxSession({7});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));

    scheduler_.runNow(+10s);
    const auto timeout   = 1s;
    const auto send_time = now();

    const auto             payload = makeIotaArray<UDPARD_MTU_DEFAULT_MAX_SINGLE_FRAME + 1>(b('0'));
    const TransferMetadata metadata{0x13, send_time, Priority::Nominal};

    auto failure = session->send(metadata, makeSpansFrom(payload));
    EXPECT_THAT(failure, Eq(cetl::nullopt));

    {
        const InSequence s;

        EXPECT_CALL(tx_socket_mock_, send(_, _, _, _))
            .WillOnce([&](auto deadline, auto endpoint, auto, auto fragments) {
                EXPECT_THAT(now(), send_time + 10us);
                EXPECT_THAT(deadline, send_time + timeout);
                EXPECT_THAT(endpoint.ip_address, 0xEF000007);
                EXPECT_THAT(fragments, SizeIs(1));
                EXPECT_THAT(fragments[0], SizeIs(24 + UDPARD_MTU_DEFAULT_MAX_SINGLE_FRAME + 4));
                return ITxSocket::SendResult::Success{true /*is_accepted*/};
            });
        EXPECT_CALL(tx_socket_mock_, send(_, _, _, _))
            .WillOnce([&](auto deadline, auto endpoint, auto, auto fragments) {
                EXPECT_THAT(now(), send_time + 10us);
                EXPECT_THAT(deadline, send_time + timeout);
                EXPECT_THAT(endpoint.ip_address, 0xEF000007);
                EXPECT_THAT(fragments, SizeIs(1));
                // NB! No `+4` here b/c CRC was in the start frame.
                EXPECT_THAT(fragments[0], SizeIs(24 + 1));
                return ITxSocket::SendResult::Success{true /*is_accepted*/};
            });
    }

    scheduler_.runNow(+10us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    scheduler_.runNow(+10us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_F(TestUpdTransport, send_multiframe_payload_to_redundant_not_ready_media)
{
    StrictMock<MediaMock>    media_mock2{};
    StrictMock<TxSocketMock> tx_socket_mock2{"TxS2"};
    StrictMock<RxSocketMock> rx_socket_mock2{"RxS2"};
    EXPECT_CALL(tx_socket_mock2, getMtu()).WillRepeatedly(Return(UDPARD_MTU_DEFAULT));
    EXPECT_CALL(rx_socket_mock2, receive()).WillRepeatedly(Invoke([]() { return cetl::nullopt; }));
    EXPECT_CALL(media_mock2, makeTxSocket()).WillRepeatedly(Invoke([this, &tx_socket_mock2]() {
        return libcyphal::detail::makeUniquePtr<TxSocketMock::ReferenceWrapper::Spec>(mr_, tx_socket_mock2);
    }));
    EXPECT_CALL(media_mock2, makeRxSocket(_)).WillRepeatedly(Invoke([&](auto&) {
        return libcyphal::detail::makeUniquePtr<RxSocketMock::ReferenceWrapper::Spec>(mr_, rx_socket_mock2);
    }));

    auto transport = makeTransport({mr_}, &media_mock2);
    EXPECT_THAT(transport->setLocalNodeId(0x45), Eq(cetl::nullopt));

    auto maybe_session = transport->makeMessageTxSession({7});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));

    scheduler_.runNow(+10s);
    const auto timeout   = 1s;
    const auto send_time = now();

    const auto             payload = makeIotaArray<UDPARD_MTU_DEFAULT>(b('0'));
    const TransferMetadata metadata{0x13, send_time, Priority::Nominal};

    auto failure = session->send(metadata, makeSpansFrom(payload));
    EXPECT_THAT(failure, Eq(cetl::nullopt));

    {
        const InSequence s;

        auto expectSocketCalls = [&](TxSocketMock& tx_socket_mock, const std::string& ctx, TimePoint when) {
            EXPECT_CALL(tx_socket_mock, send(_, _, _, _))
                .WillOnce([&, ctx, when](auto deadline, auto endpoint, auto, auto fragments) {
                    EXPECT_THAT(now(), when) << ctx;
                    EXPECT_THAT(deadline, send_time + timeout) << ctx;
                    EXPECT_THAT(endpoint.ip_address, 0xEF000007);
                    EXPECT_THAT(fragments, SizeIs(1));
                    EXPECT_THAT(fragments[0], SizeIs(24 + UDPARD_MTU_DEFAULT_MAX_SINGLE_FRAME + 4));
                    return ITxSocket::SendResult::Success{true /*is_accepted*/};
                });
            EXPECT_CALL(tx_socket_mock, send(_, _, _, _))
                .WillOnce([&, ctx, when](auto deadline, auto endpoint, auto, auto fragments) {
                    EXPECT_THAT(now(), when) << ctx;
                    EXPECT_THAT(deadline, send_time + timeout) << ctx;
                    EXPECT_THAT(endpoint.ip_address, 0xEF000007);
                    EXPECT_THAT(fragments, SizeIs(1));
                    EXPECT_THAT(fragments[0], SizeIs(24 + 4));
                    return ITxSocket::SendResult::Success{true /*is_accepted*/};
                });
        };

        // Emulate once that the first media is not ready to send fragment (@10us). So transport will
        // switch to the second media, and only on the next run will (@20us) retry with the first media again.
        //
        EXPECT_CALL(tx_socket_mock_, send(_, _, _, _)).WillOnce([&](auto, auto, auto, auto) {
            EXPECT_THAT(now(), send_time + 10us);
            return ITxSocket::SendResult::Success{false /*is_accepted*/};
        });
        expectSocketCalls(tx_socket_mock2, "M#2", send_time + 10us);
        expectSocketCalls(tx_socket_mock_, "M#1", send_time + 20us);
    }

    scheduler_.runNow(+10us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    scheduler_.runNow(+10us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    scheduler_.runNow(+10us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
}

TEST_F(TestUpdTransport, send_payload_to_redundant_fallible_media)
{
    using SocketSendReport = IUdpTransport::TransientErrorReport::MediaTxSocketSend;

    StrictMock<MediaMock>    media_mock2{};
    StrictMock<TxSocketMock> tx_socket_mock2{"S2"};
    StrictMock<RxSocketMock> rx_socket_mock2{"RxS2"};
    EXPECT_CALL(tx_socket_mock2, getMtu()).WillRepeatedly(Return(UDPARD_MTU_DEFAULT));
    EXPECT_CALL(rx_socket_mock2, receive()).WillRepeatedly(Invoke([]() { return cetl::nullopt; }));
    EXPECT_CALL(media_mock2, makeTxSocket()).WillRepeatedly(Invoke([&]() {
        return libcyphal::detail::makeUniquePtr<TxSocketMock::ReferenceWrapper::Spec>(mr_, tx_socket_mock2);
    }));
    EXPECT_CALL(media_mock2, makeRxSocket(_)).WillRepeatedly(Invoke([&](auto&) {
        return libcyphal::detail::makeUniquePtr<RxSocketMock::ReferenceWrapper::Spec>(mr_, rx_socket_mock2);
    }));

    auto transport = makeTransport({mr_}, &media_mock2);
    EXPECT_THAT(transport->setLocalNodeId(0x45), Eq(cetl::nullopt));

    auto maybe_session = transport->makeMessageTxSession({7});
    ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(NotNull()));
    auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));

    scheduler_.runNow(+10s);
    const auto send_time = now();

    const auto             payload = makeIotaArray<6>(b('0'));
    const TransferMetadata metadata{0x13, send_time, Priority::Nominal};

    // First attempt to send payload.
    auto failure = session->send(metadata, makeSpansFrom(payload));
    EXPECT_THAT(failure, Eq(cetl::nullopt));

    // 1st run: media #0 and there is no transient error handler; its frame should be dropped.
    {
        EXPECT_CALL(tx_socket_mock_, send(_, _, _, _)).WillOnce(Return(ArgumentError{}));

        scheduler_.runNow(+10us, [&] {
            EXPECT_THAT(transport->run(now()), UbVariantWith<AnyFailure>(VariantWith<ArgumentError>(_)));
        });
    }
    // 2nd run: media #1 and transient error handler have failed; its frame should be dropped.
    {
        StrictMock<TransientErrorHandlerMock> handler_mock{};
        transport->setTransientErrorHandler(std::ref(handler_mock));
        EXPECT_CALL(handler_mock, invoke(VariantWith<SocketSendReport>(Truly([&](auto& report) {
                        EXPECT_THAT(report.failure, VariantWith<ArgumentError>(_));
                        EXPECT_THAT(report.media_index, 1);
                        auto culprit = static_cast<TxSocketMock::ReferenceWrapper&>(report.culprit);
                        EXPECT_THAT(culprit.tx_socket_mock(), Ref(tx_socket_mock2));
                        return true;
                    }))))
            .WillOnce(Return(PlatformError{MyPlatformError{13}}));

        EXPECT_CALL(tx_socket_mock2, send(_, _, _, _)).WillOnce(Return(ArgumentError{}));

        scheduler_.runNow(+10us, [&] {
            EXPECT_THAT(transport->run(now()), UbVariantWith<AnyFailure>(VariantWith<PlatformError>(_)));
        });

        // No frames should be left in the session.
        scheduler_.runNow(+10us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    }

    // Second attempt to send payload.
    EXPECT_THAT(session->send(metadata, makeSpansFrom(payload)), Eq(cetl::nullopt));

    // 3rd run: media #0 has failed but transient error handler succeeded.
    {
        StrictMock<TransientErrorHandlerMock> handler_mock{};
        transport->setTransientErrorHandler(std::ref(handler_mock));
        EXPECT_CALL(handler_mock, invoke(VariantWith<SocketSendReport>(Truly([&](auto& report) {
                        EXPECT_THAT(report.failure, VariantWith<ArgumentError>(_));
                        EXPECT_THAT(report.media_index, 0);
                        auto culprit = static_cast<TxSocketMock::ReferenceWrapper&>(report.culprit);
                        EXPECT_THAT(culprit.tx_socket_mock(), Ref(tx_socket_mock_));
                        return true;
                    }))))
            .WillOnce(Return(cetl::nullopt));

        EXPECT_CALL(tx_socket_mock_, send(_, _, _, _)).WillOnce(Return(ArgumentError{}));
        EXPECT_CALL(tx_socket_mock2, send(_, _, _, _))
            .WillOnce(Return(ITxSocket::SendResult::Success{true /*is_accepted*/}));

        scheduler_.runNow(+10us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });

        // No frames should be left in the session.
        scheduler_.runNow(+10us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    }
}

/// Transport should not attempt to re-create any TX sockets if there is nothing to send,
/// even if there is a "passive, never sending" TX session alive with "faulty" TX socket.
///
TEST_F(TestUpdTransport, no_adhoc_tx_sockets_creation_at_run_when_there_is_nothing_to_send)
{
    auto transport = makeTransport({mr_});

    // Ignore all transient errors.
    StrictMock<TransientErrorHandlerMock> handler_mock{};
    transport->setTransientErrorHandler(std::ref(handler_mock));
    EXPECT_CALL(handler_mock, invoke(_)).WillRepeatedly(Return(cetl::nullopt));

    // 1. Nothing to send, so no need to create any TX sockets.
    {
        EXPECT_CALL(media_mock_, makeTxSocket()).Times(0);

        scheduler_.runNow(+10us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
        scheduler_.runNow(+10us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
        scheduler_.runNow(+10us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    }
    // 2. Still no need to create any TX sockets, even with a "passive, never sending" TX session alive
    {
        // One attempt still expected (b/c of the session creation), but not on every `transport::run`.
        EXPECT_CALL(media_mock_, makeTxSocket()).WillOnce(Return(MemoryError{}));

        auto maybe_session = transport->makeMessageTxSession({7});
        ASSERT_THAT(maybe_session, VariantWith<UniquePtr<IMessageTxSession>>(NotNull()));
        auto session = cetl::get<UniquePtr<IMessageTxSession>>(std::move(maybe_session));

        scheduler_.runNow(+10us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
        scheduler_.runNow(+10us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
        scheduler_.runNow(+10us, [&] { EXPECT_THAT(transport->run(now()), UbVariantWithoutValue()); });
    }
}

// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace

namespace cetl
{

// Just random id: 96B4BFC0-FCDD-4804-9C0E-97566FD9BE42
template <>
constexpr type_id type_id_getter<MyPlatformError>() noexcept
{
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
    return {0x96, 0xB4, 0xBF, 0xC0, 0xFC, 0xDD, 0x48, 0x04, 0x9C, 0x0E, 0x97, 0x56, 0x6F, 0xD9, 0xBE, 0x42};
}

}  // namespace cetl
