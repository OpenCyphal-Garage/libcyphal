/// @file
/// libcyphal common header.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_GTEST_HELPERS_HPP
#define LIBCYPHAL_GTEST_HELPERS_HPP

#include <libcyphal/types.hpp>
#include <libcyphal/transport/types.hpp>

#include <ostream>
#include <gtest/gtest-printers.h>

// MARK: - GTest Printers:

namespace std
{
#if (__cplusplus >= CETL_CPP_STANDARD_17)
inline std::ostream& operator<<(std::ostream& os, const std::byte& b)
{
    return os << "0x" << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<std::uint32_t>(b);
}
#endif
#if (__cplusplus >= CETL_CPP_STANDARD_20)
template <typename T>
inline std::ostream& operator<<(std::ostream& os, const std::span<T>& items)
{
    os << "{size=" << items.size() << ", data=[";
    for (const auto& item : items)
    {
        os << testing::PrintToString(item) << ", ";
    }
    return os << ")}";
}
#endif
}  // namespace std

namespace cetl
{
namespace pf17
{
inline std::ostream& operator<<(std::ostream& os, const cetl::byte& b)
{
    return os << "0x" << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<std::uint32_t>(b);
}
}  // namespace pf17
namespace pf20
{
template <typename T>
inline std::ostream& operator<<(std::ostream& os, const cetl::span<T>& items)
{
    os << "{size=" << items.size() << ", data=[";
    for (const auto& item : items)
    {
        os << testing::PrintToString(item) << ", ";
    }
    return os << ")}";
}
}  // namespace pf20
}  // namespace cetl

namespace libcyphal
{

inline void PrintTo(const Duration duration, std::ostream* os)
{
    auto locale = os->imbue(std::locale("en_US"));
    *os << std::chrono::duration_cast<std::chrono::microseconds>(duration).count() << "_us";
    os->imbue(locale);
}

inline void PrintTo(const TimePoint time_point, std::ostream* os)
{
    PrintTo(time_point.time_since_epoch(), os);
}

namespace transport
{

inline void PrintTo(const Priority priority, std::ostream* os)
{
    static constexpr std::array<const char*, 8> names{
        "Exceptional (0)",
        "Immediate (1)",
        "Fast (2)",
        "High (3)",
        "Nominal (4)",
        "Low (5)",
        "Slow (6)",
        "Optional (7)",
    };
    *os << names[static_cast<std::size_t>(priority)];
}

}  // namespace transport

// MARK: - GTest Matchers:

namespace transport
{
namespace can
{

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
        const auto priority = static_cast<Priority>((can_id >> 26) & 0b111);
        if (os)
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
        const auto is_service = (can_id & 1 << 25) != 0;
        if (os)
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
        const auto subject_id = (can_id >> 8) & CANARD_SUBJECT_ID_MAX;
        if (os)
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

class FrameLastByteMatcher
{
public:
    using is_gtest_matcher = void;

    explicit FrameLastByteMatcher(TransferId transfer_id, bool is_start, bool is_end, bool is_toggle)
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
        const auto is_start    = (byte_value & 1 << 7) != 0;
        const auto is_end      = (byte_value & 1 << 6) != 0;
        const auto is_toggle   = (byte_value & 1 << 5) != 0;
        if (os)
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
inline testing::Matcher<const cetl::byte&> FrameLastByteEq(TransferId transfer_id,
                                                           bool       is_start  = true,
                                                           bool       is_end    = true,
                                                           bool       is_toggle = true)
{
    return FrameLastByteMatcher(transfer_id, is_start, is_end, is_toggle);
}

}  // namespace can
}  // namespace transport
}  // namespace libcyphal

#endif  // LIBCYPHAL_GTEST_HELPERS_HPP
