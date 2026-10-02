// A host stand-in for the Teensy core, for the firmware simulator.
//
// The real src/main.cpp, MotionMGR.cpp, SerialCMDReader.cpp, Helpers.cpp and
// DiodeLaserCtrl.cpp compile against it on a PC. Time is simulated: the
// harness advances sim::now_ns, and every clock the firmware reads (millis(),
// the cycle counter behind nanos()) derives from it. Pin writes, galvo
// positions and serial lines are recorded with their time, so the tests and
// the benchmark can replay what the laser and the mirrors did.
#pragma once

#include <math.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

typedef uint8_t byte;

#define OUTPUT 1
#define INPUT 0
#define HIGH 1
#define LOW 0
#ifndef F_CPU
#define F_CPU 600000000
#endif

namespace sim {

extern uint64_t now_ns;

// One pin write, in the order the firmware made them.
enum class PinOp { Mode, Digital, Analog };
struct PinEvent {
  uint64_t t;
  int pin;
  PinOp op;
  int value;
};
extern std::vector<PinEvent> pin_events;
extern int analog_resolution_bits;
extern double analog_frequency_hz;
extern uint64_t analog_frequency_set_ns;

// The galvo position the firmware commanded, in 16-bit counts.
struct GalvoEvent {
  uint64_t t;
  uint16_t x;
  uint16_t y;
};
extern std::vector<GalvoEvent> galvo_events;

void reset();

}  // namespace sim

// The Teensy cycle counter, 600 MHz, 32 bits: it wraps every 7.16 s, as on
// the board. Helpers.cpp nanos() widens it to 64 bits.
uint32_t sim_cycle_counter();
#define ARM_DWT_CYCCNT (sim_cycle_counter())

unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);
void pinMode(int pin, int mode);
void digitalWrite(int pin, int value);
void analogWrite(int pin, int value);
void analogWriteResolution(int bits);
void analogWriteFrequency(int pin, double hz);
float tempmonGetTemp();

class String {
 public:
  String() {}
  String(const char *s) : s_(s ? s : "") {}
  const char *c_str() const { return s_.c_str(); }

 private:
  std::string s_;
};

// A serial port. The harness puts bytes in with an arrival time; the
// firmware sees a byte only once its time has come. Every line the firmware
// prints is kept with the time it was finished.
class SimSerial {
 public:
  struct Line {
    uint64_t t;
    std::string text;
  };
  std::deque<std::pair<uint64_t, uint8_t>> in;
  std::vector<Line> lines;
  std::string partial;

  void begin(long) {}
  void deliver(const std::string &text, uint64_t at_ns) {
    for (unsigned char c : text) in.emplace_back(at_ns, c);
  }
  int available() { return (!in.empty() && in.front().first <= sim::now_ns) ? 1 : 0; }
  int read() {
    if (!available()) return -1;
    int c = in.front().second;
    in.pop_front();
    return c;
  }
  void print(const char *s) { write_text(s); }
  void print(const String &s) { write_text(s.c_str()); }
  void print(int v) { write_text(std::to_string(v).c_str()); }
  void println(const char *s) { write_text(s); write_text("\n"); }
  void println(int v) { print(v); write_text("\n"); }
  void println(double v) { write_text(std::to_string(v).c_str()); write_text("\n"); }
  void flush() {}

 private:
  void write_text(const char *s) {
    for (; *s; ++s) {
      if (*s == '\n') {
        lines.push_back({sim::now_ns, partial});
        partial.clear();
      } else {
        partial += *s;
      }
    }
  }
};

extern SimSerial Serial;
extern SimSerial Serial5;
