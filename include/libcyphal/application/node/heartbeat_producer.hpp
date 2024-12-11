/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef LIBCYPHAL_APPLICATION_NODE_HEARTBEAT_PRODUCER_HPP_INCLUDED
#define LIBCYPHAL_APPLICATION_NODE_HEARTBEAT_PRODUCER_HPP_INCLUDED

#include "libcyphal/config.hpp"
#include "libcyphal/executor.hpp"
#include "libcyphal/presentation/presentation.hpp"
#include "libcyphal/presentation/publisher.hpp"
#include "libcyphal/transport/transport.hpp"
#include "libcyphal/types.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pmr/function.hpp>

#include <uavcan/node/Heartbeat_1_0.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace libcyphal
{
namespace application
{
namespace node
{

/// @brief Defines 'Heartbeat' producer component for the application node.
///
/// Internally, it uses the 'Heartbeat' message publisher to periodically publish heartbeat messages.
///
/// No Sonar cpp:S3624 "Customize this class' destructor to participate in resource management."
/// We need custom move constructor to reset up the publishing callback,
/// but at the destructor level, we don't need to do anything.
///
class HeartbeatProducer final  // NOSONAR cpp:S3624
{
public:
    /// @brief Defines the message type for the Heartbeat.
    ///
    using Message = uavcan::node::Heartbeat_1_0;

    /// @brief Factory method to create a Heartbeat instance.
    ///
    /// @param presentation The presentation layer instance. In use to create 'Heartbeat' publisher.
    /// @return The Heartbeat instance or a failure.
    ///
    static auto make(presentation::Presentation& presentation)
        -> Expected<HeartbeatProducer, presentation::Presentation::MakeFailure>
    {
        auto maybe_heartbeat_pub = presentation.makePublisher<Publisher::Message>();
        if (auto* const failure = cetl::get_if<presentation::Presentation::MakeFailure>(&maybe_heartbeat_pub))
        {
            return std::move(*failure);
        }

        return HeartbeatProducer{presentation, cetl::get<Publisher>(std::move(maybe_heartbeat_pub))};
    }

    HeartbeatProducer(HeartbeatProducer&& other) noexcept
        : presentation_{other.presentation_}
        , startup_time_{other.startup_time_}
        , publisher_{other.publisher_}
        , message_{other.message_}
        , update_callback_fn_{std::move(other.update_callback_fn_)}
        , next_exec_time_{other.next_exec_time_}

    {
        // We can't move `periodic_cb_` callback (b/c it captures its own `this` pointer),
        // so we need to stop it in the moved-from object, and start in the new one.
        other.stopPublishing();
        startPublishing();
    }

    ~HeartbeatProducer() = default;

    HeartbeatProducer(const HeartbeatProducer&)                = delete;
    HeartbeatProducer& operator=(const HeartbeatProducer&)     = delete;
    HeartbeatProducer& operator=(HeartbeatProducer&&) noexcept = delete;

    /// @brief Gets reference to the Heartbeat message instance.
    ///
    /// Could be used to setup the message data. Initially, the message has default values.
    /// `Message.uptime` field is periodically updated by this producer to reflect duration
    /// since the node startup, so user should not modify it - it will be overridden on the next update.
    /// As an alternative, the user can set the update callback to modify the message before it is published.
    ///
    Message& message() noexcept
    {
        return message_;
    }

    /// @brief Umbrella type for heartbeat update entities.
    ///
    struct UpdateCallback
    {
        /// @brief Defines standard arguments for heartbeat update callback.
        ///
        struct Arg
        {
            /// Holds the current heartbeat message.
            Message& message;

            /// Holds the approximate time when the callback was called.
            TimePoint approx_now;
        };

        /// @brief Defines signature of the heartbeat update callback function.
        ///
        static constexpr auto FunctionSize = config::Application::Node::HeartbeatProducer_UpdateCallback_FunctionSize();
        using Function                     = cetl::pmr::function<void(const Arg& arg), FunctionSize>;
    };

    /// @brief Sets the message update callback for the heartbeat.
    ///
    /// As an alternative, the user can modify the `message()` directly - the next periodic update will reflect.
    ///
    /// @param update_callback_fn The update callback function, which will be called before publication of the next
    ///                           heartbeat message. Allows the user to modify the `message` before it will be
    ///                           published. `arg.message.uptime` field is automatically prepopulated to reflect
    ///                           duration since the node startup, so although the field could be modified by the user,
    ///                           it will be overridden anyway on the next update.
    ///
    void setUpdateCallback(UpdateCallback::Function&& update_callback_fn)
    {
        update_callback_fn_ = std::move(update_callback_fn);
    }

private:
    using Callback  = IExecutor::Callback;
    using Publisher = presentation::Publisher<Message>;

    HeartbeatProducer(presentation::Presentation& presentation, Publisher&& publisher)
        : presentation_{presentation}
        , startup_time_{presentation.executor().now()}
        , publisher_{std::move(publisher)}
        , message_{Message::allocator_type{&presentation.memory()}}
        , next_exec_time_{startup_time_}
    {
        startPublishing();
    }

    static constexpr Duration getPeriod()
    {
        return std::chrono::seconds(1);
    }

    void startPublishing()
    {
        periodic_cb_ = presentation_.executor().registerCallback([this](const auto& arg) {
            //
            // We keep track of the next execution time to allow
            // smooth rescheduling to the new instance in the move constructor.
            next_exec_time_ = arg.exec_time + getPeriod();

            publishMessage(arg.approx_now);
        });

        const auto result = periodic_cb_.schedule(Callback::Schedule::Repeat{next_exec_time_, getPeriod()});
        CETL_DEBUG_ASSERT(result, "");
        (void) result;
    }

    void publishMessage(const TimePoint approx_now)
    {
        // Publishing of heartbeats makes sense only if the local node ID is known.
        if (presentation_.transport().getLocalNodeId() == cetl::nullopt)
        {
            return;
        }

        // Prepopulate "uptime" field, which is the time elapsed since the node was started.
        // The update callback function (if any) is allowed to modify the message before it is published.
        //
        const auto uptime_in_secs = std::chrono::duration_cast<std::chrono::seconds>(approx_now - startup_time_);
        message_.uptime           = static_cast<std::uint32_t>(uptime_in_secs.count());
        if (update_callback_fn_)
        {
            update_callback_fn_(UpdateCallback::Arg{message_, approx_now});
        }

        // Deadline for the next publication is the current time plus 1s publication period -
        // it has no sense to keep the message in the queue for longer than that.
        // There is nothing we can do about possible publishing failures - we just ignore them.
        // TODO: Introduce error handler at the node level.
        (void) publisher_.publish(approx_now + getPeriod(), message_);
    }

    void stopPublishing()
    {
        periodic_cb_.reset();
    }

    // MARK: Data members:

    presentation::Presentation& presentation_;
    const TimePoint             startup_time_;
    const Publisher             publisher_;
    Callback::Any               periodic_cb_;
    Message                     message_;
    UpdateCallback::Function    update_callback_fn_;
    TimePoint                   next_exec_time_;

};  // HeartbeatProducer

}  // namespace node
}  // namespace application
}  // namespace libcyphal

#endif  // LIBCYPHAL_APPLICATION_NODE_HEARTBEAT_PRODUCER_HPP_INCLUDED
