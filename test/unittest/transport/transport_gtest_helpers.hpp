/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_TRANSPORT_GTEST_HELPERS_HPP_INCLUDED
#define LIBCYPHAL_TRANSPORT_GTEST_HELPERS_HPP_INCLUDED

#include <libcyphal/transport/msg_sessions.hpp>
#include <libcyphal/transport/scattered_buffer.hpp>
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

inline void PrintTo(const Priority priority, std::ostream* os)
{
    switch (priority)
    {
    case Priority::Exceptional:
        *os << "Exceptional(0)";
        break;
    case Priority::Immediate:
        *os << "Immediate(1)";
        break;
    case Priority::Fast:
        *os << "Fast(2)";
        break;
    case Priority::High:
        *os << "High(3)";
        break;
    case Priority::Nominal:
        *os << "Nominal(4)";
        break;
    case Priority::Low:
        *os << "Low(5)";
        break;
    case Priority::Slow:
        *os << "Slow(6)";
        break;
    case Priority::Optional:
        *os << "Optional(7)";
        break;
    }
}

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

inline void PrintTo(const TransferMetadata& meta, std::ostream* os)
{
    *os << "TransferMetadata{transfer_id=" << meta.transfer_id << ", priority=" << testing::PrintToString(meta.priority)
        << "}";
}

inline void PrintTo(const TransferRxMetadata& meta, std::ostream* os)
{
    *os << "TransferRxMetadata{base=" << testing::PrintToString(meta.base)
        << ", timestamp=" << testing::PrintToString(meta.timestamp) << "}";
}

inline void PrintTo(const TransferTxMetadata& meta, std::ostream* os)
{
    *os << "TransferTxMetadata{base=" << testing::PrintToString(meta.base)
        << ", deadline=" << testing::PrintToString(meta.deadline) << "}";
}

inline void PrintTo(const ServiceRxMetadata& meta, std::ostream* os)
{
    *os << "SvcRxMetadata{rx_meta=" << testing::PrintToString(meta.rx_meta)
        << ", remote_node_id=" << meta.remote_node_id << "}";
}

inline void PrintTo(const ServiceTxMetadata& meta, std::ostream* os)
{
    *os << "SvcTxMetadata{tx_meta=" << testing::PrintToString(meta.tx_meta)
        << ", remote_node_id=" << meta.remote_node_id << "}";
}

inline void PrintTo(const ScatteredBuffer& buffer, std::ostream* os)
{
    *os << "ScatteredBuffer{size=" << buffer.size() << "}";
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

class ServiceRxMetadataMatcher
{
public:
    using is_gtest_matcher = void;

    explicit ServiceRxMetadataMatcher(const ServiceRxMetadata& meta)
        : meta_{meta}
    {
    }

    bool MatchAndExplain(const ServiceRxMetadata& meta, std::ostream*) const
    {
        return meta.rx_meta.base.transfer_id == meta_.rx_meta.base.transfer_id &&
               meta.rx_meta.base.priority == meta_.rx_meta.base.priority &&
               meta.rx_meta.timestamp == meta_.rx_meta.timestamp && meta.remote_node_id == meta_.remote_node_id;
    }
    void DescribeTo(std::ostream* os) const
    {
        *os << "is " << testing::PrintToString(meta_);
    }
    void DescribeNegationTo(std::ostream* os) const
    {
        *os << "is NOT " << testing::PrintToString(meta_);
    }

private:
    const ServiceRxMetadata meta_;
};
inline testing::Matcher<const ServiceRxMetadata&> ServiceRxMetadataEq(const ServiceRxMetadata& meta)
{
    return ServiceRxMetadataMatcher(meta);

}  // ServiceRxMetadataMatcher

class TransferTxMetadataMatcher
{
public:
    using is_gtest_matcher = void;

    explicit TransferTxMetadataMatcher(const TransferTxMetadata& meta)
        : meta_{meta}
    {
    }

    bool MatchAndExplain(const TransferTxMetadata& meta, std::ostream*) const
    {
        return meta.base.transfer_id == meta_.base.transfer_id && meta.base.priority == meta_.base.priority &&
               meta.deadline == meta_.deadline;
    }
    void DescribeTo(std::ostream* os) const
    {
        *os << "is " << testing::PrintToString(meta_);
    }
    void DescribeNegationTo(std::ostream* os) const
    {
        *os << "is NOT " << testing::PrintToString(meta_);
    }

private:
    const TransferTxMetadata meta_;
};
inline testing::Matcher<const TransferTxMetadata&> TransferTxMetadataEq(const TransferTxMetadata& meta)
{
    return TransferTxMetadataMatcher(meta);

}  // TransferTxMetadataMatcher

class ServiceTxMetadataMatcher
{
public:
    using is_gtest_matcher = void;

    explicit ServiceTxMetadataMatcher(const ServiceTxMetadata& meta)
        : meta_{meta}
    {
    }

    bool MatchAndExplain(const ServiceTxMetadata& meta, std::ostream*) const
    {
        return testing::Value(meta.tx_meta, TransferTxMetadataEq(meta_.tx_meta)) &&
               meta.remote_node_id == meta_.remote_node_id;
    }
    void DescribeTo(std::ostream* os) const
    {
        *os << "is " << testing::PrintToString(meta_);
    }
    void DescribeNegationTo(std::ostream* os) const
    {
        *os << "is NOT " << testing::PrintToString(meta_);
    }

private:
    const ServiceTxMetadata meta_;
};
inline testing::Matcher<const ServiceTxMetadata&> ServiceTxMetadataEq(const ServiceTxMetadata& meta)
{
    return ServiceTxMetadataMatcher(meta);

}  // ServiceTxMetadataMatcher

}  // namespace transport
}  // namespace libcyphal

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

#endif  // LIBCYPHAL_TRANSPORT_GTEST_HELPERS_HPP_INCLUDED
