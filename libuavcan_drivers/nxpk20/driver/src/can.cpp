/*
 * Teensy 3.2 header for UAVCAN
 * @author fwindolf - Florian Windolf  florianwindolf@gmail.com
 */

#include <uavcan_nxpk20/can.hpp>
#include <uavcan_nxpk20/clock.hpp>


namespace uavcan_nxpk20
{

// Init static variable
CanDriver CanDriver::self;


// initialize can driver
void CanIface::init(const IfaceParams& p)
{
  // set mailbox and buffer sizes
  flexcan->setTxBufferSize(p.tx_buff_size);
  flexcan->setRxBufferSize(p.rx_buff_size);
  
  // default filter and mask
  CAN_filter_t start_filter;
  start_filter.id = 0;
  uint32_t start_mask = p.dis_all_RX_by_default ? 0xFFFFFFFF : 0;

  // // start flexcan interface
  flexcan->begin(p.bitrate, start_filter, start_mask, p.use_alt_tx_pin, p.use_alt_rx_pin);
}

// sends a CAN frame
int16_t CanIface::send(const CanFrame& frame, MonotonicTime tx_deadline, CanIOFlags flags)
{
  // Frame was not transmitted until tx deadline
  if(!tx_deadline.isZero() && clock::getMonotonic() >= tx_deadline)
  {
    return -1;
  }

  // IMPORTANT: there is no further deadline checking from here on!
  // This may cause the message to be stored in flexcan ring buffer and
  // send after deadline.

  CAN_message_t msg;
  msg.id = frame.id;
  msg.flags.extended = frame.isExtended();
  msg.flags.remote = frame.isRemoteTransmissionRequest();
  msg.len = frame.dlc;
  for(int i=0; i<frame.dlc; i++)
  {
    msg.buf[i] = frame.data[i];
  }

  return flexcan->write(msg);
}

// receives a CAN frame
int16_t CanIface::receive(CanFrame& out_frame, MonotonicTime& out_ts_monotonic, UtcTime& out_ts_utc,
                           CanIOFlags& out_flags)
{

  CAN_message_t msg;

  if(!flexcan->read(msg))
  {
    return 0;
  }

  // save timestamp
  out_ts_monotonic = clock::getMonotonic();
  out_ts_utc = UtcTime();               // TODO: change to clock::getUtc() when properly implemented

  out_frame.id = msg.id;
  if(msg.flags.extended)
  {
    out_frame.id &= uavcan::CanFrame::MaskExtID;
    out_frame.id |= uavcan::CanFrame::FlagEFF;
  }

  out_frame.dlc = msg.len;
  for(int i=0; i<msg.len; i++)
  {
    out_frame.data[i] = msg.buf[i];
  }

  return 1;
}



int16_t CanIface::configureFilters(const CanFilterConfig* filter_configs,
                                   uint16_t num_configs)
{
  // loop over all boxes
  for(int i=0; i<num_configs; i++)
  {
    // set filter
    CAN_filter_t f;
    f.id = filter_configs[i].id;
    f.flags.extended = filter_configs[i].id & uavcan::CanFrame::FlagEFF;
    f.flags.remote =   filter_configs[i].id & uavcan::CanFrame::FlagRTR;
    flexcan->setFilter(f, i);

    // set mask
    flexcan->setMask(filter_configs[i].mask, i);
  }
  return 0;
}

uint64_t CanIface::getErrorCount() const
{
  return flexcan->rxBufferOverruns();
}

uint16_t CanIface::getNumFilters() const
{
  // one filter for each RX mailbox possible
  return flexcan->getNumRxBoxes();
}


bool CanIface::availableToReadMsg() const
{
  return flexcan->available() > 0;
}

bool CanIface::availableToSendMsg() const
{

  return flexcan->freeTxBuffer();
}

void CanDriver::init(const IfaceParams* params)
{
   #ifndef INCLUDE_FLEXCAN_CAN1
    can0.init(params[0]);
  #else
    can0.init(params[0]);
    can1.init(params[1]);
  #endif
}


ICanIface* CanDriver::getIface(uint8_t iface_index)
{
  #ifndef INCLUDE_FLEXCAN_CAN1
    return &can0;
  #else
    if(0 == iface_index){
      return &can0;
    }else if(1 == iface_index)
    {
      return &can1;
    }
  #endif

  return NULL;
}

uint8_t CanDriver::getNumIfaces() const
{
  #ifndef INCLUDE_FLEXCAN_CAN1
    return 1;
  #else
    return 2;
  #endif
}

int16_t CanDriver::select(CanSelectMasks& inout_masks,
                          const CanFrame* (&)[MaxCanIfaces],
                          MonotonicTime blocking_deadline)
{

  while(clock::getMonotonic() < blocking_deadline ||
        blocking_deadline.isZero())
  {
    uint8_t readyDevices = 0;

    #ifndef INCLUDE_FLEXCAN_CAN1
      inout_masks.read = can0.availableToReadMsg() ? 1:0 << 0;
    #else
      inout_masks.read  = can0.availableToReadMsg() ? 1:0 << 0;
      inout_masks.read |= can1.availableToReadMsg() ? 1:0 << 1;
    #endif

    #ifndef INCLUDE_FLEXCAN_CAN1
      inout_masks.write = can0.availableToSendMsg() ? 1:0 << 0;
    #else
      inout_masks.write  = can0.availableToSendMsg() > 0 ? 1:0 << 0;
      inout_masks.write |= can1.availableToSendMsg() > 0 ? 1:0 << 1;
    #endif

    #ifndef INCLUDE_FLEXCAN_CAN1
      if(1 == inout_masks.read || 1 == inout_masks.write)
      {
        readyDevices = 1;
      }
    #else
      if(3 == inout_masks.read || 3 == inout_masks.write){
        readyDevices = 2;
      }else if(1 == inout_masks.read || 1 == inout_masks.write){
        readyDevices = 1;
      }
    #endif

    // if blocking_deadline is zero -> non blocking operation
    if(readyDevices > 0 || blocking_deadline.isZero())
    {
      return readyDevices;
    }

  }

  // deadline passed
  return -1;

}

} // uavcan_nxpk20
