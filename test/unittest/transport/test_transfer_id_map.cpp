/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "transfer_id_storage_mock.hpp"

#include <libcyphal/transport/transfer_id_map.hpp>
#include <libcyphal/transport/types.hpp>

#include <cetl/pf17/cetlpf.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <limits>

namespace
{

using namespace libcyphal::transport;  // NOLINT This our main concern here in the unit tests.

using testing::Eq;
using testing::Optional;
using testing::StrictMock;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class TestTransferIdMapAndGens : public testing::Test
{};

// MARK: - Tests:

TEST_F(TestTransferIdMapAndGens, trivial_default)
{
    StrictMock<detail::TransferIdStorageMock> transfer_id_storage_mock;

    const detail::TrivialTransferIdGenerator tf_id_gen{transfer_id_storage_mock};

    EXPECT_CALL(transfer_id_storage_mock, load()).WillOnce(testing::Return(42));
    EXPECT_CALL(transfer_id_storage_mock, save(43));
    EXPECT_THAT(tf_id_gen.nextTransferId(), 42);

    EXPECT_CALL(transfer_id_storage_mock, load()).WillOnce(testing::Return(43));
    EXPECT_CALL(transfer_id_storage_mock, save(44));
    EXPECT_THAT(tf_id_gen.nextTransferId(), 43);

    EXPECT_CALL(transfer_id_storage_mock, load()).WillOnce(testing::Return(44));
    EXPECT_CALL(transfer_id_storage_mock, save(45));
    EXPECT_THAT(tf_id_gen.nextTransferId(), 44);
}

TEST_F(TestTransferIdMapAndGens, trivial_max_tf_id)
{
    StrictMock<detail::TransferIdStorageMock> transfer_id_storage_mock;

    const detail::TrivialTransferIdGenerator tf_id_gen{transfer_id_storage_mock};

    // The starting value is 2^64 - 2.
    constexpr auto max = std::numeric_limits<TransferId>::max();
    EXPECT_CALL(transfer_id_storage_mock, load()).WillOnce(testing::Return(max - 2));
    EXPECT_CALL(transfer_id_storage_mock, save(max - 1));
    EXPECT_THAT(tf_id_gen.nextTransferId(), max - 2);

    EXPECT_CALL(transfer_id_storage_mock, load()).WillOnce(testing::Return(max - 1));
    EXPECT_CALL(transfer_id_storage_mock, save(max));
    EXPECT_THAT(tf_id_gen.nextTransferId(), max - 1);

    EXPECT_CALL(transfer_id_storage_mock, load()).WillOnce(testing::Return(max));
    EXPECT_CALL(transfer_id_storage_mock, save(0));
    EXPECT_THAT(tf_id_gen.nextTransferId(), max);

    EXPECT_CALL(transfer_id_storage_mock, load()).WillOnce(testing::Return(0));
    EXPECT_CALL(transfer_id_storage_mock, save(1));
    EXPECT_THAT(tf_id_gen.nextTransferId(), 0);
}

TEST_F(TestTransferIdMapAndGens, small_range_with_default_map)
{
    detail::DefaultTransferIdStorage default_transfer_id_storage{9};

    detail::SmallRangeTransferIdGenerator<8> tf_id_gen{4, default_transfer_id_storage};

    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(1));
    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(2));
    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(3));
    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(0));
    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(1));

    tf_id_gen.retainTransferId(1);

    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(2));
    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(3));
    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(0));
    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(2));

    tf_id_gen.retainTransferId(2);

    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(3));
    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(0));
    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(3));
    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(0));

    tf_id_gen.retainTransferId(0);

    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(3));
    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(3));

    tf_id_gen.retainTransferId(3);

    EXPECT_THAT(tf_id_gen.nextTransferId(), Eq(cetl::nullopt));
    EXPECT_THAT(tf_id_gen.nextTransferId(), Eq(cetl::nullopt));

    tf_id_gen.releaseTransferId(2);

    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(2));
    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(2));

    tf_id_gen.releaseTransferId(0);

    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(0));
    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(2));
    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(0));

    tf_id_gen.releaseTransferId(1);

    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(1));
    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(2));
    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(0));
    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(1));

    tf_id_gen.releaseTransferId(3);

    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(2));
    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(3));
    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(0));
    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(1));
    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(2));
    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(3));
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
