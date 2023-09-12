/// @copyright Copyright Amazon.com Inc. and its affiliates. All Rights Reserved.
/// @file
/// Cyphal Interface for Subscribers

#ifndef LIBCYPHAL_PRESENTATION_SUBSCRIBER_HPP_INCLUDED
#define LIBCYPHAL_PRESENTATION_SUBSCRIBER_HPP_INCLUDED

#include "libcyphal/transport/id_types.hpp"
#include "libcyphal/types/status.hpp"

namespace libcyphal
{
namespace presentation
{

class ISubscriber
{
public:
    /// @brief Registers interest in an incoming subject ID
    /// @param[in] subject_id Subject ID to register
    /// @return Status of registering the subject ID
    virtual Status registerSubjectID(PortID subject_id) = 0;

    ISubscriber()                         = default;
    ISubscriber(ISubscriber&)             = delete;
    ISubscriber& operator=(ISubscriber&)  = delete;
    ISubscriber& operator=(ISubscriber&&) = delete;

protected:
    virtual ~ISubscriber() = default;
};

/// @brief Implementation for the Subscriber
class Subscriber 
{
public: 
    /// @brief Constructor
    /// @param transport The concrete underlying transport provided by the user
    Subscriber(libcyphal::Transport& transport)
        : transport_{&transport}
    { 
    }

    /// @brief Move Constructor
    /// @param[in] other Subscriber to move from
    Subscriber(Subscriber&& other) noexcept
        : transport_{other.transport_}
    {
        other.transport_ = nullptr;
    }

    /// @brief Registers interest in an incoming subject ID
    /// @param[in] subject_id Subject ID to register
    /// @return Status of registering the subject ID
    Status registerSubjectID(PortID subject_id)
    {
        if (transport_ == nullptr)
        {
            return ResultCode::NotInitialized;
        }

        return transport_->registerSubscription(subject_id, transport::TransferKindMessage);
    }

private:
    libcyphal::Transport* transport_{nullptr};
};

}  // namespace presentation
}  // namespace libcyphal

#endif  // LIBCYPHAL_PRESENTATION_SUBSCRIBER_HPP_INCLUDED