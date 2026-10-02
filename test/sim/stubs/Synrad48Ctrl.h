// main.h includes the Synrad CO2 driver. The OSLS-1 has a diode
// (LASER_IS_DIODE), so the simulator needs only the name.
#pragma once
#include <LaserController.h>

class Synrad48Ctrl : public LaserController {
 public:
  void begin(int, int) {}
  void stop() {}
  void update(uint16_t) {}
  void update() {}
  bool isHalted() { return true; }
};
