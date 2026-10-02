// The galvo driver for the simulator. The board clocks each position out as
// an XY2-100 frame in an interrupt. Here setPos records the commanded counts
// with the simulated time, which is all the timing analysis needs.
#pragma once
#include <Arduino.h>

class XY2_100 {
 public:
  void begin() {}
  void setPos(uint16_t x, uint16_t y) { sim::galvo_events.push_back({sim::now_ns, x, y}); }
};
