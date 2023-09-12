/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Cyphal Interface for Publishers

#ifndef LIBCYPHAL_PRESENTATION_PUBLISHER_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_PUBLISHER_HPP_INCLUDED

#include "libcyphal/transport/id_types.hpp"
#include "libcyphal/types/span.hpp"
#include "libcyphal/types/status.hpp"
#include "libcyphal/transport.hpp"

namespace libcyphal
{
namespace presentation
{

class IPublisher
{
public:
    /// @brief Registers an outgoing subject ID
    /// @param[in] subject_id Subject ID to register
    /// @return Status of registering the subject ID
    virtual Status registerSubjectID(PortID subject_id) = 0;

    /// @brief Sends a serialized message
    /// @param[in] subject_id Subject ID of the message
    /// @param[in] payload The serialized message
    /// @param[in] size Size of the message
    virtual Status publish(const PortID subject_id, const std::uint8_t* payload, const std::size_t size) = 0;

    IPublisher()                        = default;
    IPublisher(IPublisher&)             = delete;
    IPublisher& operator=(IPublisher&)  = delete;
    IPublisher& operator=(IPublisher&&) = delete;

protected:
    virtual ~IPublisher() = default;
};

/// @brief Implementation for the Publisher
class Publisher : public IPublisher
{
public:
    /// @brief Publisher Constructor
    /// @param transport The concrete transport passed in from the user
    Publisher(libcyphal::Transport& transport)
        : transport_{&transport}
    {
    }

    /// @brief Move Constructor
    /// @param[in] other Publisher object to move from
    Publisher(Publisher&& other) noexcept
        : transport_{other.transport_}
    {
        other.transport_ = nullptr;
    }

    /// @brief Registers an outgoing subject ID
    /// @param[in] subject_id Subject ID to register
    /// @return Status of registering the subject ID
    Status registerSubjectID(PortID subject_id) override
    {
        if (transport_ == nullptr)
        {
            return ResultCode::NotInitialized;
        }

        return transport_->registerPublication(subject_id, transport::TransferKindMessage);
    }

    /// @brief Sends a serialized message
    /// @param[in] subject_id Subject ID of the message
    /// @param[in] payload The serialized message
    /// @param[in] size Size of the message
    Status publish(const PortID subject_id, const std::uint8_t* payload, const std::size_t size) override
    {
        if (transport_ == nullptr)
        {
            return ResultCode::NotInitialized;
        }

        Span<const std::uint8_t> span_message{payload, size};
        return transport_->broadcast(subject_id, span_message);
    }

private:
    libcyphal::Transport* transport_{nullptr};
};

}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_PUBLISHER_HPP_INCLUDED