/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_GTEST_HELPERS_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_GTEST_HELPERS_HPP_INCLUDED

#include <libcyphal/transport/msg_sessions.hpp>
#include <libcyphal/transport/svc_sessions.hpp>
#include <libcyphal/transport/types.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest-matchers.h>
#include <gtest/gtest-printers.h>

#include <ostream>

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

namespace libcyphal
{
namespace transport
{

// MARK: - GTest Printers:

inline void PrintTo(const MessageRxParams& params, std::ostream* os)
{
    *os << "MessageRxParams{extent_bytes=" << params.extent_bytes;
    *os << ", subject_id=" << params.subject_id << "}";
}

inline void PrintTo(const MessageTxParams& params, std::ostream* os)
{
    *os << "MessageTxParams{subject_id=" << params.subject_id << "}";
}

inline void PrintTo(const RequestRxParams& params, std::ostream* os)
{
    *os << "RequestRxParams{extent_bytes=" << params.extent_bytes;
    *os << ", service_id=" << params.service_id << "}";
}

inline void PrintTo(const RequestTxParams& params, std::ostream* os)
{
    *os << "RequestTxParams{service_id=" << params.service_id;
    *os << ", server_node_id=" << params.server_node_id << "}";
}

inline void PrintTo(const ResponseRxParams& params, std::ostream* os)
{
    *os << "ResponseRxParams{extent_bytes=" << params.extent_bytes;
    *os << ", service_id=" << params.service_id;
    *os << ", server_node_id=" << params.server_node_id << "}";
}

inline void PrintTo(const ResponseTxParams& params, std::ostream* os)
{
    *os << "ResponseTxParams{service_id=" << params.service_id << "}";
}

// MARK: - GTest Matchers:

class MessageRxParamsMatcher
{
public:
    using is_gtest_matcher = void;

    explicit MessageRxParamsMatcher(const MessageRxParams& params)
        : params_{params}
    {
    }

    bool MatchAndExplain(const MessageRxParams& params, std::ostream*) const
    {
        return params.extent_bytes == params_.extent_bytes && params.subject_id == params_.subject_id;
    }
    void DescribeTo(std::ostream* os) const
    {
        *os << "is " << testing::PrintToString(params_);
    }
    void DescribeNegationTo(std::ostream* os) const
    {
        *os << "is NOT " << testing::PrintToString(params_);
    }

private:
    const MessageRxParams params_;
};
inline testing::Matcher<const MessageRxParams&> MessageRxParamsEq(const MessageRxParams& params)
{
    return MessageRxParamsMatcher(params);

}  // MessageRxParamsMatcher

class MessageTxParamsMatcher
{
public:
    using is_gtest_matcher = void;

    explicit MessageTxParamsMatcher(const MessageTxParams& params)
        : params_{params}
    {
    }

    bool MatchAndExplain(const MessageTxParams& params, std::ostream*) const
    {
        return params.subject_id == params_.subject_id;
    }
    void DescribeTo(std::ostream* os) const
    {
        *os << "is " << testing::PrintToString(params_);
    }
    void DescribeNegationTo(std::ostream* os) const
    {
        *os << "is NOT " << testing::PrintToString(params_);
    }

private:
    const MessageTxParams params_;
};
inline testing::Matcher<const MessageTxParams&> MessageTxParamsEq(const MessageTxParams& params)
{
    return MessageTxParamsMatcher(params);

}  // MessageTxParamsMatcher

class RequestRxParamsMatcher
{
public:
    using is_gtest_matcher = void;

    explicit RequestRxParamsMatcher(const RequestRxParams& params)
        : params_{params}
    {
    }

    bool MatchAndExplain(const RequestRxParams& params, std::ostream*) const
    {
        return params.extent_bytes == params_.extent_bytes && params.service_id == params_.service_id;
    }
    void DescribeTo(std::ostream* os) const
    {
        *os << "is " << testing::PrintToString(params_);
    }
    void DescribeNegationTo(std::ostream* os) const
    {
        *os << "is NOT " << testing::PrintToString(params_);
    }

private:
    const RequestRxParams params_;
};
inline testing::Matcher<const RequestRxParams&> RequestRxParamsEq(const RequestRxParams& params)
{
    return RequestRxParamsMatcher(params);

}  // RequestRxParamsMatcher

class RequestTxParamsMatcher
{
public:
    using is_gtest_matcher = void;

    explicit RequestTxParamsMatcher(const RequestTxParams& params)
        : params_{params}
    {
    }

    bool MatchAndExplain(const RequestTxParams& params, std::ostream*) const
    {
        return params.service_id == params_.service_id && params.server_node_id == params_.server_node_id;
    }
    void DescribeTo(std::ostream* os) const
    {
        *os << "is " << testing::PrintToString(params_);
    }
    void DescribeNegationTo(std::ostream* os) const
    {
        *os << "is NOT " << testing::PrintToString(params_);
    }

private:
    const RequestTxParams params_;
};
inline testing::Matcher<const RequestTxParams&> RequestTxParamsEq(const RequestTxParams& params)
{
    return RequestTxParamsMatcher(params);

}  // RequestTxParamsMatcher

class ResponseRxParamsMatcher
{
public:
    using is_gtest_matcher = void;

    explicit ResponseRxParamsMatcher(const ResponseRxParams& params)
        : params_{params}
    {
    }

    bool MatchAndExplain(const ResponseRxParams& params, std::ostream*) const
    {
        return params.extent_bytes == params_.extent_bytes && params.service_id == params_.service_id &&
               params.server_node_id == params_.server_node_id;
    }
    void DescribeTo(std::ostream* os) const
    {
        *os << "is " << testing::PrintToString(params_);
    }
    void DescribeNegationTo(std::ostream* os) const
    {
        *os << "is NOT " << testing::PrintToString(params_);
    }

private:
    const ResponseRxParams params_;
};
inline testing::Matcher<const ResponseRxParams&> ResponseRxParamsEq(const ResponseRxParams& params)
{
    return ResponseRxParamsMatcher(params);

}  // ResponseRxParamsMatcher

class ResponseTxParamsMatcher
{
public:
    using is_gtest_matcher = void;

    explicit ResponseTxParamsMatcher(const ResponseTxParams& params)
        : params_{params}
    {
    }

    bool MatchAndExplain(const ResponseTxParams& params, std::ostream*) const
    {
        return params.service_id == params_.service_id;
    }
    void DescribeTo(std::ostream* os) const
    {
        *os << "is " << testing::PrintToString(params_);
    }
    void DescribeNegationTo(std::ostream* os) const
    {
        *os << "is NOT " << testing::PrintToString(params_);
    }

private:
    const ResponseTxParams params_;
};
inline testing::Matcher<const ResponseTxParams&> ResponseTxParamsEq(const ResponseTxParams& params)
{
    return ResponseTxParamsMatcher(params);

}  // ResponseTxParamsMatcher

}  // namespace transport
}  // namespace libcyphal

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

#endif  // LIBCYPHAL_TRANSPORT_GTEST_HELPERS_HPP_INCLUDED
