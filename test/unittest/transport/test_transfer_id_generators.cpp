/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <libcyphal/transport/transfer_id_generators.hpp>
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

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class TestTransferIdGenerators : public testing::Test
{};

// MARK: - Tests:

TEST_F(TestTransferIdGenerators, trivial_default)
{
    // Default starting value is 0.
    detail::TrivialTransferIdGenerator tf_id_gen;

    EXPECT_THAT(tf_id_gen.nextTransferId(), 0);
    EXPECT_THAT(tf_id_gen.nextTransferId(), 1);
    EXPECT_THAT(tf_id_gen.nextTransferId(), 2);
    EXPECT_THAT(tf_id_gen.nextTransferId(), 3);
    EXPECT_THAT(tf_id_gen.nextTransferId(), 4);
    EXPECT_THAT(tf_id_gen.nextTransferId(), 5);
}

TEST_F(TestTransferIdGenerators, trivial_max_tf_id)
{
    // Starting value is 2^64 - 3.
    constexpr auto max = std::numeric_limits<TransferId>::max();

    detail::TrivialTransferIdGenerator tf_id_gen;
    tf_id_gen.setNextTransferId(max - 3);

    EXPECT_THAT(tf_id_gen.nextTransferId(), max - 3);
    EXPECT_THAT(tf_id_gen.nextTransferId(), max - 2);
    EXPECT_THAT(tf_id_gen.nextTransferId(), max - 1);
    EXPECT_THAT(tf_id_gen.nextTransferId(), max);
    EXPECT_THAT(tf_id_gen.nextTransferId(), 0);
    EXPECT_THAT(tf_id_gen.nextTransferId(), 1);
    EXPECT_THAT(tf_id_gen.nextTransferId(), 2);
}

TEST_F(TestTransferIdGenerators, small_range)
{
    detail::SmallRangeTransferIdGenerator<8> tf_id_gen{4};

    EXPECT_THAT(tf_id_gen.nextTransferId(), Optional(0));
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
