/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_CAN_GTEST_HELPERS_HPP
#define LIBCYPHAL_TRANSPORT_CAN_GTEST_HELPERS_HPP

#include <canard.h>
#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/transport/can/can_transport_impl.hpp>
#include <libcyphal/transport/can/media.hpp>
#include <libcyphal/transport/types.hpp>
#include <libcyphal/types.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest-matchers.h>
#include <gtest/gtest-printers.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <ios>
#include <ostream>

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

namespace libcyphal
{
namespace transport
{
namespace can
{

// MARK: - GTest Printers:

inline void PrintTo(const Filter& filter, std::ostream* os)
{
    *os << std::uppercase << std::hex;
    *os << "{id=0x" << std::setw(8) << std::setfill('0') << filter.id;
    *os << ", mask=0x" << std::setw(8) << std::setfill('0') << filter.mask << "}";
}

// MARK: - GTest Matchers:

class PriorityOfCanIdMatcher
{
public:
    using is_gtest_matcher = void;

    explicit PriorityOfCanIdMatcher(Priority priority)
        : priority_{priority}
    {
    }

    bool MatchAndExplain(const CanId& can_id, std::ostream* os) const
    {
        const auto priority = static_cast<Priority>((can_id >> 26UL) & 0b111UL);
        if (os != nullptr)
        {
            *os << "priority='" << testing::PrintToString(priority) << "'";
        }
        return priority == priority_;
    }
    void DescribeTo(std::ostream* os) const
    {
        *os << "priority=='" << testing::PrintToString(priority_) << "'";
    }
    void DescribeNegationTo(std::ostream* os) const
    {
        *os << "priority!='" << testing::PrintToString(priority_) << "'";
    }

private:
    const Priority priority_;
};
inline testing::Matcher<const CanId&> PriorityOfCanIdEq(Priority priority)
{
    return PriorityOfCanIdMatcher(priority);
}

class IsServiceCanIdMatcher
{
public:
    using is_gtest_matcher = void;

    explicit IsServiceCanIdMatcher(bool is_service)
        : is_service_{is_service}
    {
    }

    bool MatchAndExplain(const CanId& can_id, std::ostream* os) const
    {
        const auto is_service = (can_id & (1UL << 25UL)) != 0;
        if (os != nullptr)
        {
            *os << "is_service=" << std::boolalpha << is_service;
        }
        return is_service == is_service_;
    }
    void DescribeTo(std::ostream* os) const
    {
        *os << "is_service==" << std::boolalpha << is_service_;
    }
    void DescribeNegationTo(std::ostream* os) const
    {
        *os << "is_service!=" << std::boolalpha << is_service_;
    }

private:
    const bool is_service_;
};
inline testing::Matcher<const CanId&> IsServiceCanId(bool is_service = true)
{
    return IsServiceCanIdMatcher(is_service);
}
inline testing::Matcher<const CanId&> IsMessageCanId(bool is_message = true)
{
    return IsServiceCanIdMatcher(!is_message);
}

class SubjectOfCanIdMatcher
{
public:
    using is_gtest_matcher = void;

    explicit SubjectOfCanIdMatcher(PortId subject_id)
        : subject_id_{subject_id}
    {
    }

    bool MatchAndExplain(const CanId& can_id, std::ostream* os) const
    {
        const auto subject_id = (can_id >> 8UL) & CANARD_SUBJECT_ID_MAX;
        if (os != nullptr)
        {
            *os << "subject_id=" << subject_id;
        }
        return subject_id == subject_id_;
    }
    void DescribeTo(std::ostream* os) const
    {
        *os << "subject_id==" << subject_id_;
    }
    void DescribeNegationTo(std::ostream* os) const
    {
        *os << "subject_id!=" << subject_id_;
    }

private:
    const PortId subject_id_;
};
inline testing::Matcher<const CanId&> SubjectOfCanIdEq(PortId subject_id)
{
    return SubjectOfCanIdMatcher(subject_id);
}

class ServiceOfCanIdMatcher
{
public:
    using is_gtest_matcher = void;

    explicit ServiceOfCanIdMatcher(PortId service_id)
        : service_id_{service_id}
    {
    }

    bool MatchAndExplain(const CanId& can_id, std::ostream* os) const
    {
        const auto service_id = (can_id >> 14UL) & CANARD_SERVICE_ID_MAX;
        if (os != nullptr)
        {
            *os << "service_id=" << service_id;
        }
        return service_id == service_id_;
    }
    void DescribeTo(std::ostream* os) const
    {
        *os << "service_id==" << service_id_;
    }
    void DescribeNegationTo(std::ostream* os) const
    {
        *os << "service_id!=" << service_id_;
    }

private:
    const PortId service_id_;
};
inline testing::Matcher<const CanId&> ServiceOfCanIdEq(PortId service_id)
{
    return ServiceOfCanIdMatcher(service_id);
}

class SourceNodeCanIdMatcher
{
public:
    using is_gtest_matcher = void;

    explicit SourceNodeCanIdMatcher(NodeId node_id)
        : node_id_{node_id}
    {
    }

    bool MatchAndExplain(const CanId& can_id, std::ostream* os) const
    {
        const auto node_id = can_id & CANARD_NODE_ID_MAX;
        if (os != nullptr)
        {
            *os << "node_id=" << node_id;
        }
        return node_id == node_id_;
    }
    void DescribeTo(std::ostream* os) const
    {
        *os << "node_id==" << node_id_;
    }
    void DescribeNegationTo(std::ostream* os) const
    {
        *os << "node_id!=" << node_id_;
    }

private:
    const NodeId node_id_;
};
inline testing::Matcher<const CanId&> SourceNodeOfCanIdEq(NodeId node_id)
{
    return SourceNodeCanIdMatcher(node_id);
}

class DestinationNodeCanIdMatcher
{
public:
    using is_gtest_matcher = void;

    explicit DestinationNodeCanIdMatcher(NodeId node_id)
        : node_id_{node_id}
    {
    }

    bool MatchAndExplain(const CanId& can_id, std::ostream* os) const
    {
        const auto node_id = (can_id >> 7UL) & CANARD_NODE_ID_MAX;
        if (os != nullptr)
        {
            *os << "node_id=" << node_id;
        }
        return node_id == node_id_;
    }
    void DescribeTo(std::ostream* os) const
    {
        *os << "node_id==" << node_id_;
    }
    void DescribeNegationTo(std::ostream* os) const
    {
        *os << "node_id!=" << node_id_;
    }

private:
    const NodeId node_id_;
};
inline testing::Matcher<const CanId&> DestinationNodeOfCanIdEq(NodeId node_id)
{
    return DestinationNodeCanIdMatcher(node_id);
}

class TailByteMatcher
{
public:
    using is_gtest_matcher = void;

    explicit TailByteMatcher(TransferId transfer_id, bool is_start, bool is_end, bool is_toggle)
        : transfer_id_{static_cast<std::uint8_t>(transfer_id & CANARD_TRANSFER_ID_MAX)}
        , is_start_{is_start}
        , is_end_{is_end}
        , is_toggle_{is_toggle}
    {
    }

    bool MatchAndExplain(const cetl::byte& last_byte, std::ostream* os) const
    {
        const auto byte_value  = static_cast<std::uint8_t>(last_byte);
        const auto transfer_id = byte_value & CANARD_TRANSFER_ID_MAX;
        const auto is_start    = (byte_value & (1UL << 7UL)) != 0;
        const auto is_end      = (byte_value & (1UL << 6UL)) != 0;
        const auto is_toggle   = (byte_value & (1UL << 5UL)) != 0;
        if (os != nullptr)
        {
            *os << "{";
            *os << "transfer_id=" << static_cast<cetl::byte>(transfer_id_) << ", ";
            *os << "is_start=" << std::boolalpha << is_start << ", ";
            *os << "is_end=" << std::boolalpha << is_end << ", ";
            *os << "is_toggle=" << std::boolalpha << is_toggle;
            *os << "}";
        }
        return transfer_id == transfer_id_ && is_start == is_start_ && is_end == is_end_ && is_toggle == is_toggle_;
    }
    void DescribeTo(std::ostream* os) const
    {
        *os << "{";
        *os << "transfer_id=" << static_cast<cetl::byte>(transfer_id_) << ", ";
        *os << "is_start=" << std::boolalpha << is_start_ << ", ";
        *os << "is_end=" << std::boolalpha << is_end_ << ", ";
        *os << "is_toggle=" << std::boolalpha << is_toggle_;
        *os << "}";
    }
    void DescribeNegationTo(std::ostream* os) const
    {
        *os << "is NOT equal to ";
        DescribeTo(os);
    }

private:
    const std::uint8_t transfer_id_;
    const bool         is_start_;
    const bool         is_end_;
    const bool         is_toggle_;
};
inline testing::Matcher<const cetl::byte&> TailByteEq(TransferId transfer_id,
                                                      bool       is_start  = true,
                                                      bool       is_end    = true,
                                                      bool       is_toggle = true)
{
    return TailByteMatcher(transfer_id, is_start, is_end, is_toggle);
}

class FilterMatcher
{
public:
    using is_gtest_matcher = void;

    explicit FilterMatcher(const Filter& filter)
        : filter_{filter}
    {
    }

    bool MatchAndExplain(const Filter& filter, std::ostream* os) const
    {
        if (os != nullptr)
        {
            *os << testing::PrintToString(filter);
        }
        return filter.id == filter_.id && filter.mask == filter_.mask;
    }
    void DescribeTo(std::ostream* os) const
    {
        *os << testing::PrintToString(filter_);
    }
    void DescribeNegationTo(std::ostream* os) const
    {
        *os << "is NOT equal to ";
        DescribeTo(os);
    }

private:
    const Filter filter_;
};
inline testing::Matcher<const Filter&> FilterEq(const Filter& filter)
{
    return FilterMatcher(filter);
}

}  // namespace can
}  // namespace transport
}  // namespace libcyphal

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

#endif  // LIBCYPHAL_TRANSPORT_CAN_GTEST_HELPERS_HPP
