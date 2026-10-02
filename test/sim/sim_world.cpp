// The simulated Teensy: clock, pins, serial ports. See stubs/Arduino.h.
#include <Arduino.h>

namespace sim {
uint64_t now_ns = 0;
std::vector<PinEvent> pin_events;
std::vector<GalvoEvent> galvo_events;
int analog_resolution_bits = 8;
double analog_frequency_hz = 0;
uint64_t analog_frequency_set_ns = 0;

void reset() {
  now_ns = 0;
  pin_events.clear();
  galvo_events.clear();
  analog_resolution_bits = 8;
  analog_frequency_hz = 0;
  analog_frequency_set_ns = 0;
  Serial = SimSerial();
  Serial5 = SimSerial();
}
}  // namespace sim

SimSerial Serial;
SimSerial Serial5;

static const uint64_t NS_PER_MS = 1000000;
static const uint64_t NS_PER_US = 1000;

uint32_t sim_cycle_counter() {
  // 600 cycles per microsecond: cycles = ns * 0.6.
  return (uint32_t)((sim::now_ns * 3) / 5);
}

unsigned long millis() { return (unsigned long)(sim::now_ns / NS_PER_MS); }
unsigned long micros() { return (unsigned long)(sim::now_ns / NS_PER_US); }
// The board blocks; the simulated clock moves on by the same time.
void delay(unsigned long ms) { sim::now_ns += (uint64_t)ms * NS_PER_MS; }
// The firmware rewrites the laser pin on every pass of loop(). A write that
// repeats the last one for that pin changes nothing, so it is not kept.
static void record(int pin, sim::PinOp op, int value) {
  for (auto it = sim::pin_events.rbegin(); it != sim::pin_events.rend(); ++it) {
    if (it->pin != pin) continue;
    if (it->op == op && it->value == value) return;
    break;
  }
  sim::pin_events.push_back({sim::now_ns, pin, op, value});
}
void pinMode(int pin, int mode) { record(pin, sim::PinOp::Mode, mode); }
void digitalWrite(int pin, int value) { record(pin, sim::PinOp::Digital, value); }
void analogWrite(int pin, int value) { record(pin, sim::PinOp::Analog, value); }
void analogWriteResolution(int bits) { sim::analog_resolution_bits = bits; }
void analogWriteFrequency(int, double hz) {
  sim::analog_frequency_hz = hz;
  sim::analog_frequency_set_ns = sim::now_ns;
}
float tempmonGetTemp() { return 42.0f; }

// lib/LaserController/LaserController.h declares isHalted() virtual but gives
// it no body. The Teensy build at -O2 never needs the base class's table; an
// unoptimised build does. Every laser class overrides it.
#include <LaserController.h>
bool LaserController::isHalted() { return true; }
