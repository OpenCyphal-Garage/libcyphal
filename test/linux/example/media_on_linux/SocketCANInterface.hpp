/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#include <queue>
#include <memory>

#include "libuavcan/libuavcan.hpp"
#include "libuavcan/transport/media/interfaces.hpp"
#include "libuavcan/transport/media/can.hpp"

namespace libuavcan
{
/**
 * @defgroup examples Examples
 *
 * @{
 * @file
 * @namespace example   Namespace containing example implementations of libuavcan.
 */
namespace example
{
using CanFrame = libuavcan::transport::media::CAN::Frame<libuavcan::transport::media::CAN::Type2_0::MaxFrameSizeBytes>;
using CanInterface = libuavcan::transport::media::Interface<CanFrame>;

/**
 * Example of a media::Interface implemented for <a
 * href="https://www.kernel.org/doc/Documentation/networking/can.txt">SocketCAN</a>.
 */
class SocketCANInterface : public CanInterface
{
private:
    struct TxQueueItem
    {
        /**
         * Copy frames into the system heap when in the priority queue to prevent
         * unnecessary payload copies.
         */
        std::unique_ptr<CanFrame>  frame;
        libuavcan::time::Monotonic deadline;

        TxQueueItem(const CanFrame& frame, libuavcan::time::Monotonic deadline)
            : frame(new CanFrame(frame))
            , deadline(deadline)
        {}
        TxQueueItem(TxQueueItem&& rhs)
            : frame(std::move(rhs.frame))
            , deadline(rhs.deadline)
        {}
        ~TxQueueItem()       = default;
        TxQueueItem& operator=(TxQueueItem&& rhs)
        {
            frame    = std::move(rhs.frame);
            deadline = rhs.deadline;
            return *this;
        }
        bool operator<(const TxQueueItem& rhs) const
        {
            if (frame->priorityLowerThan(*rhs.frame))
            {
                return true;
            }
            return false;
        }
        TxQueueItem(const TxQueueItem&) = delete;
        TxQueueItem& operator=(const TxQueueItem&) = delete;
    };
    const std::uint_fast8_t          index_;
    const int                        fd_;
    std::priority_queue<TxQueueItem> tx_queue_;
    std::queue<CanFrame>             rx_queue_;

public:
    SocketCANInterface(std::uint_fast8_t index, int fd);

    virtual ~SocketCANInterface();

    /**
     * Provide time for this object to read and write messages to and from
     * RX and TX queues.
     */
    void execute();

    // +----------------------------------------------------------------------+
    // | CanInterface
    // +----------------------------------------------------------------------+
    virtual std::uint_fast8_t getInterfaceIndex() const override;

    virtual libuavcan::Result sendOrEnqueue(const CanFrame& frame, libuavcan::time::Monotonic tx_deadline) override;

    virtual libuavcan::Result sendOrEnqueue(const CanFrame& frame) override;

    virtual libuavcan::Result receive(CanFrame& out_frame) override;

private:
    /**
     * dequeue and send one frame from the tx_queue_ if there are any.
     */
    libuavcan::Result writeNextFrame();

    libuavcan::Result readOneFrameIntoQueueIfAvailable();
};

}  // namespace example
/** @} */  // end of examples group
}  // namespace libuavcan
