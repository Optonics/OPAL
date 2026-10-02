// The firmware simulator: the real setup() and loop() of src/main.cpp on a
// PC, a model of the printer host that streams the G-code, and a model of
// what the laser output pin and the mirrors do with the result.
//
// What is modelled, and why:
// - The loop: each pass of loop() costs loop_ns of simulated time. The board
//   also spends most of its time in the 4 MHz XY2-100 interrupt, so a pass is
//   microseconds, not nanoseconds. The real figure needs a scope (bench item).
// - The laser output: a duty change written with analogWrite() reaches the pin
//   at the start of the next PWM period. That is how the Teensy 4 FlexPWM
//   reloads its compare registers. At the firmware's 1 kHz, a period is 1 ms.
//   pinMode(OUTPUT) plus digitalWrite() switch the pin to GPIO at once.
// - The mirrors: a first-order lag of mirror_tau_ns behind the commanded
//   position (0 = ideal mirrors). Real galvos track 0.1 to 0.3 ms behind.
// - The host: osls1_klipper/galvo/opal_link.py sends a line and waits for
//   "ok" before the next (window 1). Each "ok" costs host_rt_ns before the
//   next line arrives: USB, the Linux tty and Python. A larger window keeps
//   that many lines in flight.
#pragma once

#include <Arduino.h>

#include <cstdint>
#include <string>
#include <vector>

void setup();
void loop();

namespace sim {

struct HostModel {
  int window = 1;                  // lines in flight; 1 = opal_link.py today
  uint64_t rt_ns = 1000000;        // after an "ok", until the next line arrives
  // A host is not a clock: each round trip varies by up to this share, from a
  // fixed seed. Without it the run locks to the PWM period and hides latency.
  double jitter = 0.3;
  uint32_t seed = 1;
};

struct RunConfig {
  uint64_t loop_ns = 2000;         // simulated time per pass of loop()
  uint64_t host_start_ns = 1000000;
  uint64_t limit_ns = 60ULL * 1000000000ULL;
  HostModel host;
};

struct RunResult {
  bool done = false;               // "done" seen after the final M400
  uint64_t start_ns = 0;
  uint64_t end_ns = 0;
  uint64_t loops = 0;
};

// Boots the firmware (setup()) on a fresh simulated board.
void boot();
// Streams `lines` (plus a final M400) through the host model and runs loop()
// until "done" or the limit.
RunResult stream(const std::vector<std::string> &lines, const RunConfig &cfg);
// Runs loop() for `ns` of simulated time with no host traffic.
void idle(uint64_t ns, uint64_t loop_ns = 2000);
// Puts raw text on the USB port now and runs loop() until it is all read.
void send_now(const std::string &text, uint64_t loop_ns = 2000);
// How many lines the firmware printed that equal `text`.
int count_lines(const std::string &text);

// The laser pin and the mirrors as functions of time.
class Replay {
 public:
  explicit Replay(int laser_pin, double field_mm, uint64_t mirror_tau_ns = 0);
  // The beam is enabled: PWM with a duty above 0, or GPIO driven high.
  bool enabled(uint64_t t) const;
  // The pin is high at this instant (PWM high phase included).
  bool high(uint64_t t) const;
  int duty(uint64_t t) const;                  // latched PWM duty
  void commanded_mm(uint64_t t, double *x, double *y) const;
  // Mirror position, with the lag. Sampled; call in increasing t.
  void mirror_mm(uint64_t t, double *x, double *y);

 private:
  struct Change {
    uint64_t t;
    int kind;   // 0 mux to GPIO, 1 GPIO level, 2 PWM duty latched, 3 mux to PWM, 4 period restart
    int value;
  };
  std::vector<Change> changes_;
  uint64_t period_ns_ = 0;
  uint64_t period_origin_ns_ = 0;
  int duty_max_ = 4095;
  double field_mm_;
  uint64_t tau_ns_;
  double mx_ = 0, my_ = 0;
  uint64_t mt_ = 0;
  bool mirror_started_ = false;
  struct State {
    bool gpio = true;
    int level = 0;
    int duty = 0;
    uint64_t origin = 0;   // start of the PWM period count
  };
  std::vector<std::pair<uint64_t, State>> snapshots_;
  State state_at(uint64_t t) const;
};

struct Metrics {
  int lit_runs = 0;
  double late_off_mean_us = 0, late_off_max_us = 0;   // still at end, beam on
  double early_on_mean_us = 0, early_on_max_us = 0;   // beam on, not moving yet
  double dwell_total_us = 0;                          // beam enabled, mirror still
  double jump_lit_mm = 0;                             // beam high above jump_speed
  double lit_mm = 0;                                  // beam high while moving
  double dark_scan_mm = 0;                            // enabled, moving, pin low (PWM gaps)
  double layer_ms = 0;
};

// Measures a run. `still_mm_s`: below this the mirror counts as still.
// `jump_mm_s`: above this the motion is a jump, not a scan.
Metrics measure(const RunResult &run, uint64_t mirror_tau_ns, double scan_mm_s,
                double sample_ns = 1000, double window_ns = 20000);

// A hatch layer in the slicer's per-vector form (gcode/generator.py):
// M5, G0 F<jump>, M3 S<s>, G1 F<speed> per vector, then M5. Every line runs
// the same way, so each jump crosses back over the part: a beam left on
// during a jump draws a streak across it.
std::vector<std::string> hatch_layer(int vectors, double length_mm, double speed_mm_s,
                                     int s = 204, double spacing_mm = 0.1);

}  // namespace sim
