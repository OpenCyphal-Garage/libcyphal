/*
 * Teensy 3.2 header for UAVCAN
 * @author fwindolf - Florian Windolf  florianwindolf@gmail.com
 */

#pragma once

#include <uavcan/driver/system_clock.hpp>

namespace uavcan_nxpk20
{
namespace clock
{

/*
 * Starts the clock, after first init calling the function will not do anything
 */
void init();

/**
 *  Returns the elapsed MonotonicTime since init() was called
 */
uavcan::MonotonicTime getMonotonic();

/**
 * Returns UTC time if set, else zero
 */
uavcan::UtcTime getUtc();

/**
 * Adjusts the UTC time, until then getMonotonic will return zero
 */
void adjustUtc(uavcan::UtcDuration adjustment);

/**
 * Returns the clock error to previous adjustUTC, positive is hardware is slower
 */
uavcan::UtcDuration getPrevUtcAdjustment();

} // clock

class SystemClock :
  public uavcan::ISystemClock,
  public uavcan::Noncopyable
{
public:
  /**
   * Return the only instance of SystemClock, init if needed
   */
  static SystemClock& instance();


private:
  /*
   * Instance of SystemClock
   */
  static SystemClock self;

  SystemClock() { }

  uavcan::MonotonicTime getMonotonic() const override
  {
    return clock::getMonotonic();
  }

  uavcan::UtcTime getUtc() const override
  {
    return clock::getUtc();
  }

  void adjustUtc(uavcan::UtcDuration adjustment) override
  {
    clock::adjustUtc(adjustment);
  }
};


} // uavcan_nxpk20
