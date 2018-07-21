/*
 * Teensy 3.2 header for UAVCAN
 * @author fwindolf - Florian Windolf  florianwindolf@gmail.com
 */

#ifndef UAVCAN_NXPK20_CAN_HPP_INCLUDED
#define UAVCAN_NXPK20_CAN_HPP_INCLUDED


#include <uavcan/driver/can.hpp>
#include "FlexCAN.h"

using namespace uavcan;

namespace uavcan_nxpk20
{

struct IfaceParams{
  uint32_t bitrate;             // bitrate for the interface
  uint8_t tx_buff_size;         // number of elements in the ring buffer for transmit
  uint8_t rx_buff_size;         // number of elements in the ring buffer for receive
  bool use_alt_tx_pin;          // use alternative tx pin
  bool use_alt_rx_pin;          // use alternative rx pin
  bool dis_all_RX_by_default;   // disable all RX mailboxes by default via acceptance filter
};

class CanIface:
  public ICanIface,
  Noncopyable
{
private:
  static CanIface self;

  // pointer to the flexcan object
  FlexCAN* flexcan;
  
public:
  /**
   * Constructor
   */
  CanIface(FlexCAN* f)
   : flexcan(f) 
  {};

  /**
   * Returns the only reference.
   * @return Reference to this object
   */
  static CanIface& instance() { return self; }


  /**
   * Initialize the interface
   * @return None
   */
  void init(const IfaceParams& p);

  /**
   * Non-blocking transmission.
   *
   * If the frame wasn't transmitted upon TX deadline, the driver should discard it.
   *
   * Note that it is LIKELY that the library will want to send the frames that were passed into the select()
   * method as the next ones to transmit, but it is NOT guaranteed. The library can replace those with new
   * frames between the calls.
   *
   * @return 1 = one frame transmitted, 0 = TX buffer full, negative for error.
   */
  int16_t send(const CanFrame& frame, MonotonicTime tx_deadline, CanIOFlags flags) override;

  /**
   * Non-blocking reception.
   *
   * Timestamps should be provided by the CAN driver, ideally by the hardware CAN controller.
   *
   * Monotonic timestamp is required and can be not precise since it is needed only for
   * protocol timing validation (transfer timeouts and inter-transfer intervals).
   *
   * UTC timestamp is optional, if available it will be used for precise time synchronization;
   * must be set to zero if not available.
   *
   * Refer to @ref ISystemClock to learn more about timestamps.
   *
   * @param [out] out_ts_monotonic Monotonic timestamp, mandatory.
   * @param [out] out_ts_utc       UTC timestamp, optional, zero if unknown.
   * @return 1 = one frame received, 0 = RX buffer empty, negative for error.
   */
  int16_t receive(CanFrame& out_frame, MonotonicTime& out_ts_monotonic, UtcTime& out_ts_utc,
                  CanIOFlags& out_flags) override;

  /**
   * Configure the hardware CAN filters. @ref CanFilterConfig.
   *
   * @return 0 = success, negative for error.
   */
  int16_t configureFilters(const CanFilterConfig* filter_configs, uint16_t num_configs) override;

  /**
   * Number of available hardware filters.
   */
  uint16_t getNumFilters() const override;

  /**
   * Continuously incrementing counter of hardware errors.
   * Arbitration lost should not be treated as a hardware error.
   */
  uint64_t getErrorCount() const override;

  /**
   * Returns if RX has some messages ready to read
   */
  bool availableToReadMsg() const;

  /**
   * Returns available messages (TX)
   */
  bool availableToSendMsg() const;



};

/*
 * Implement the CAN Interfaces in non-redundant way
 * Singleton class
 */
class CanDriver:
  public uavcan::ICanDriver,
  uavcan::Noncopyable
{
private:
  static CanDriver self;

  #ifndef INCLUDE_FLEXCAN_CAN1
    CanIface can0;
  #else
    CanIface can0;
    CanIface can1;
  #endif

  public:

  /**
   * Constructor
   */
  #ifndef INCLUDE_FLEXCAN_CAN1
    CanDriver()
    : can0(&Can0)
    {};
  #else
    CanDriver()
    : can0(&Can0), can1(&Can1)
    {};
  #endif  

  /**
   * Returns the only reference.
   * @return Reference to this object
   */
  static CanDriver& instance() { return self; }

  /**
   * Initialize the driver
   * @param [in] array of bitrates for the interfaces
   * @return None
   */
  void init(const IfaceParams* params);

  /**
   * Returns the specified interface
   * @return Reference to Interface
   */
  ICanIface* getIface(uint8_t iface_index) override;

  /**
   * Total number of available CAN interfaces.
   * This value shall not change after initialization.
   * @return Number of available interfaces
   */
  uint8_t getNumIfaces() const override;

  /**
   * Block until the deadline, or one of the specified interfaces becomes available for read or write.
   *
   * Iface masks will be modified by the driver to indicate which exactly interfaces are available for IO.
   *
   * Bit position in the masks defines interface index.
   *
   * Note that it is allowed to return from this method even if no requested events actually happened, or if
   * there are events that were not requested by the library.
   *
   * The pending TX argument contains an array of pointers to CAN frames that the library wants to transmit
   * next, per interface. This is intended to allow the driver to properly prioritize transmissions; many
   * drivers will not need to use it. If a write flag for the given interface is set to one in the select mask
   * structure, then the corresponding pointer is guaranteed to be valid (not UAVCAN_NULLPTR).
   *
   * @param [in,out] inout_masks        Masks indicating which interfaces are needed/available for IO.
   * @param [in]     pending_tx         Array of frames, per interface, that are likely to be transmitted next.
   * @param [in]     blocking_deadline  Zero means non-blocking operation.
   * @return Positive number of ready interfaces or negative error code.
   */
  int16_t select(CanSelectMasks& inout_masks,
                 const CanFrame* (& pending_tx)[MaxCanIfaces],
                 MonotonicTime blocking_deadline) override;

};

} // uavcan_nxpk20

#endif // UAVCAN_NXPK20_CAN_HPP_INCLUDED
