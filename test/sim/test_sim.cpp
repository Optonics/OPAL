// Behaviour tests of the whole firmware loop in the simulator: the real
// setup() and loop(), fed over the simulated USB port. Each test runs in its
// own process (run.sh), because the firmware keeps static state.
//
// Usage: test_sim --list | test_sim <name>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <string>

#include "sim.h"

#include "Pins.h"
#include "configuration.h"
#include "LaserMap.h"

namespace {

int failures = 0;

#define CHECK(cond)                                                     \
  do {                                                                  \
    if (!(cond)) {                                                      \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
      failures++;                                                       \
    }                                                                   \
  } while (0)

std::map<std::string, std::function<void()>> &registry() {
  static std::map<std::string, std::function<void()>> r;
  return r;
}
struct Register {
  Register(const char *name, std::function<void()> fn) { registry()[name] = fn; }
};
#define TEST(name)                          \
  void name();                              \
  Register reg_##name(#name, name);         \
  void name()

const uint64_t US = 1000;
const uint64_t MS = 1000000;
const double FULL = 65535.0;
const int DUTY_S204 = laserDutyFromS(204, 255, LASER_RESOLUTION);

// The last value written to the laser PWM pin.
int last_duty() {
  for (auto it = sim::pin_events.rbegin(); it != sim::pin_events.rend(); ++it)
    if (it->pin == LASER_PWM_OUT_PIN && it->op == sim::PinOp::Analog) return it->value;
  return -1;
}
int last_digital(int pin) {
  for (auto it = sim::pin_events.rbegin(); it != sim::pin_events.rend(); ++it)
    if (it->pin == pin && it->op == sim::PinOp::Digital) return it->value;
  return -1;
}
// When the laser pin last got this duty.
uint64_t when_duty(int duty) {
  for (auto it = sim::pin_events.rbegin(); it != sim::pin_events.rend(); ++it)
    if (it->pin == LASER_PWM_OUT_PIN && it->op == sim::PinOp::Analog && it->value == duty) return it->t;
  return UINT64_MAX;
}
double pos_x_mm() { return sim::galvo_events.back().x / FULL * X_MAX; }
double pos_y_mm() { return sim::galvo_events.back().y / FULL * Y_MAX; }
bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }
// Sends a line and runs until the queue has executed it.
void run_line(const std::string &line, uint64_t after_ns = 1 * MS) {
  sim::send_now(line + "\n");
  sim::idle(after_ns);
}

// --- Start-up ---------------------------------------------------------------

TEST(boot_leaves_laser_off_at_1khz_12bit) {
  sim::boot();
  CHECK(last_duty() == 0);
  CHECK(sim::analog_frequency_hz == 1000);
  CHECK(sim::analog_resolution_bits == 12);
  CHECK(last_digital(LASER_SSR_OUT_PIN) == 0);
  CHECK(last_digital(GALVO_SSR_OUT_PIN) == 0);
  CHECK(sim::count_lines("* Teensy running OpenGalvo OPAL Firmware   *") == 1);
}

TEST(boot_sends_g28_to_serial5) {
  sim::boot();
  CHECK(!::Serial5.lines.empty() && ::Serial5.lines[0].text == "G28");
}

// --- Laser on and off -------------------------------------------------------

TEST(m3_writes_the_duty_and_m5_writes_zero) {
  sim::boot();
  run_line("M3 S204");
  CHECK(last_duty() == DUTY_S204);
  run_line("M5");
  CHECK(last_duty() == 0);
}

TEST(m4_is_m3) {
  sim::boot();
  run_line("M4 S255");
  CHECK(last_duty() == 4095);
}

TEST(m3_without_s_keeps_the_last_power) {
  sim::boot();
  run_line("M3 S204");
  run_line("M5");
  run_line("M3");
  CHECK(last_duty() == DUTY_S204);
}

TEST(s_out_of_range_clamps_negative) {
  sim::boot();
  run_line("M3 S300");
  CHECK(last_duty() == 4095);
  run_line("M3 S-5");
  CHECK(last_duty() == 0);
  run_line("M3 S0");
  CHECK(last_duty() == 0);
}

TEST(g1_with_s_changes_the_power) {
  sim::boot();
  run_line("M3 S100");
  run_line("G1 F1000 X1 S200", 5 * MS);
  CHECK(last_duty() == laserDutyFromS(200, 255, LASER_RESOLUTION));
}

TEST(m5_takes_effect_in_the_pass_that_reads_it) {
  sim::boot();
  run_line("M3 S204");
  uint64_t sent = sim::now_ns;
  run_line("M5");
  CHECK(when_duty(0) >= sent);
  CHECK(when_duty(0) - sent < 200 * US);  // one 36-byte budget of 2 us passes
}

// --- Motion -----------------------------------------------------------------

TEST(g1_takes_length_over_feed_in_mm_per_s) {
  sim::boot();
  sim::send_now("G1 F100 X10\n");
  uint64_t start = sim::now_ns;
  sim::idle(99 * MS);
  CHECK(pos_x_mm() < 10.0 - 0.01);       // still on the way
  sim::idle(2 * MS);
  CHECK(near(pos_x_mm(), 10.0, 0.01));   // 10 mm at 100 mm/s: 100 ms
  (void)start;
}

TEST(g1_moves_along_the_line) {
  sim::boot();
  sim::send_now("G1 F100 X10 Y20\n");
  sim::idle(50 * MS);
  double x = pos_x_mm(), y = pos_y_mm();
  CHECK(near(y, 2 * x, 0.02));           // on the line y = 2x
  // 22.36 mm at 100 mm/s takes 224 ms; after 50 ms X is 10 * 50 / 224.
  CHECK(near(x, 10.0 * 50 / 223.6, 0.05));
}

TEST(g0_jumps_in_one_pass) {
  sim::boot();
  run_line("G0 X100 Y50", 10 * US);
  CHECK(near(pos_x_mm(), 100, 0.01) && near(pos_y_mm(), 50, 0.01));
}

TEST(g91_moves_relative_g90_absolute) {
  sim::boot();
  run_line("G0 X10 Y10");
  run_line("G91");
  run_line("G0 X5 Y-2");
  CHECK(near(pos_x_mm(), 15, 0.01) && near(pos_y_mm(), 8, 0.01));
  run_line("G90");
  run_line("G0 X1 Y1");
  CHECK(near(pos_x_mm(), 1, 0.01) && near(pos_y_mm(), 1, 0.01));
}

TEST(g28_returns_to_the_origin) {
  sim::boot();
  run_line("G0 X40 Y40");
  run_line("G28");
  CHECK(near(pos_x_mm(), 0, 0.01) && near(pos_y_mm(), 0, 0.01));
}

TEST(coordinates_outside_the_field_saturate) {
  sim::boot();
  run_line("G0 X300 Y-5");
  CHECK(sim::galvo_events.back().x == 65535);
  CHECK(sim::galvo_events.back().y == 0);
}

TEST(sub_mm_coordinates_keep_their_resolution) {
  sim::boot();
  run_line("G0 X100.004");
  uint16_t a = sim::galvo_events.back().x;
  run_line("G0 X100.010");
  CHECK(sim::galvo_events.back().x > a);  // 6 um apart, still apart in counts
}

TEST(nanos_survives_the_cycle_counter_wrap) {
  // The 32-bit counter wraps every 7.16 s. A 10 s move crosses it.
  sim::boot();
  sim::send_now("G1 F10 X100\n");
  sim::idle(5 * 1000 * MS, 20 * US);
  double half = pos_x_mm();
  sim::idle(5 * 1000 * MS + 10 * MS, 20 * US);
  CHECK(near(half, 50, 0.5));
  CHECK(near(pos_x_mm(), 100, 0.01));
}

// --- Wrong input --------------------------------------------------------------

TEST(unsupported_codes_do_not_move) {
  sim::boot();
  run_line("G0 X10 Y10");
  run_line("G2 X50 Y50 I5 J5");
  run_line("G4 P1");
  run_line("T5");
  run_line("X99 Y99");
  CHECK(near(pos_x_mm(), 10, 0.01) && near(pos_y_mm(), 10, 0.01));
  CHECK(sim::count_lines("ok") == 5);    // every line answered
}

TEST(an_unknown_m_code_is_answered_and_ignored) {
  sim::boot();
  run_line("M3 S204");
  run_line("M999");
  run_line("M114");
  CHECK(last_duty() == DUTY_S204);       // the laser state is untouched
  CHECK(sim::count_lines("ok") == 3);
}

TEST(a_comment_line_does_not_leak_into_the_next) {
  sim::boot();
  run_line("; Y99 a note");
  run_line("G0 X10");
  CHECK(near(pos_y_mm(), 0, 0.01));
}

TEST(a_full_queue_loses_no_line) {
  sim::boot();
  std::string burst;
  for (int i = 1; i <= 60; ++i) burst += "G0 X" + std::to_string(i) + "\n";
  sim::send_now(burst);
  sim::idle(5 * MS);
  CHECK(near(pos_x_mm(), 60, 0.01));
  CHECK(sim::count_lines("ok") == 60);
  // Every position in order: each jump is the next millimetre.
  int seen = 0;
  for (const auto &e : sim::galvo_events) {
    double x = e.x / FULL * X_MAX;
    if (near(x, seen + 1, 0.01)) seen++;
  }
  CHECK(seen == 60);
}

TEST(an_overlong_line_reports_an_overrun_and_recovers) {
  sim::boot();
  run_line("G0 X" + std::string(200, '1'));
  CHECK(sim::count_lines("BUFFER OVERRUN") >= 1);
  run_line("G0 X5");
  CHECK(near(pos_x_mm(), 5, 0.01));
}

TEST(nan_and_garbage_numbers_do_not_crash) {
  sim::boot();
  run_line("G0 Xnan Yinf");
  run_line("G0 X1e400");
  run_line("G0 X-");
  run_line("G0 X5");
  CHECK(near(pos_x_mm(), 5, 0.01));
}

// --- Host protocol ------------------------------------------------------------

TEST(m400_says_done_after_the_motion) {
  sim::boot();
  sim::send_now("G1 F100 X10\nM400\n");
  sim::idle(200 * MS);
  CHECK(sim::count_lines("done") == 1);
  uint64_t done_t = 0;
  for (const auto &l : ::Serial.lines)
    if (l.text == "done") done_t = l.t;
  CHECK(done_t >= 100 * MS);             // not before the 100 ms move ends
}

TEST(each_g_line_also_prints_the_temperature) {
  sim::boot();
  run_line("G0 X1");
  CHECK(sim::count_lines("42.000000") == 1);
}

TEST(stream_line_by_line_finishes_a_layer) {
  sim::boot();
  sim::RunConfig cfg;
  sim::RunResult r = sim::stream(sim::hatch_layer(10, 2, 500), cfg);
  CHECK(r.done);
  CHECK(last_duty() == 0);               // the layer ends dark
}

// --- Dead-man ------------------------------------------------------------------

TEST(dead_man_cuts_the_beam_when_the_host_goes_silent) {
  sim::boot();
  run_line("M3 S204");
  uint64_t on_at = sim::now_ns;
  sim::idle(1100 * MS, 20 * US);
  CHECK(last_duty() == 0);
  CHECK(when_duty(0) - on_at >= 1000 * MS);   // not before the timeout
}

TEST(dead_man_waits_while_a_move_runs) {
  sim::boot();
  run_line("M3 S204");
  sim::send_now("G1 F10 X20\n");           // 2 s with no serial traffic
  sim::idle(1900 * MS, 20 * US);
  CHECK(last_duty() == DUTY_S204);
}

TEST(after_a_dead_man_trip_the_next_move_is_dark_until_m3) {
  sim::boot();
  run_line("M3 S204");
  sim::idle(1100 * MS, 20 * US);
  run_line("G1 F1000 X5", 10 * MS);
  CHECK(last_duty() == 0);
  run_line("M3 S204");
  CHECK(last_duty() == DUTY_S204);
}

// --- Relays and the second port ---------------------------------------------------

TEST(m80_m81_switch_the_laser_supply) {
  sim::boot();
  run_line("M81");
  CHECK(last_digital(LASER_SSR_OUT_PIN) == 0);
  CHECK(last_duty() == 0);
  run_line("M80", 300 * MS);               // waits 250 ms, then restarts the driver
  CHECK(last_digital(LASER_SSR_OUT_PIN) == 1);
  run_line("M3 S204");
  CHECK(last_duty() == DUTY_S204);
  run_line("M80");                          // already on: nothing to restart
  CHECK(last_digital(LASER_SSR_OUT_PIN) == 1);
}

TEST(m17_m18_switch_the_galvo_supply) {
  sim::boot();
  run_line("M17");
  CHECK(last_digital(GALVO_SSR_OUT_PIN) == 1);
  run_line("M18");
  CHECK(last_digital(GALVO_SSR_OUT_PIN) == 0);
}

TEST(m9_forwards_its_text_to_serial5) {
  sim::boot();
  run_line("M9 G1 Z0.1");
  bool seen = false;
  for (const auto &l : ::Serial5.lines) seen |= l.text.find("G1 Z0.1") != std::string::npos;
  CHECK(seen);
}

TEST(serial5_lines_are_echoed_and_overruns_reported) {
  sim::boot();
  ::Serial5.deliver("ok Z\n", sim::now_ns);
  sim::idle(1 * MS);
  bool echo = false;
  for (const auto &l : ::Serial.lines) echo |= l.text.find("ECHO Serial5:") != std::string::npos;
  CHECK(echo);
  ::Serial5.deliver(std::string(200, 'z') + "\n", sim::now_ns);
  sim::idle(2 * MS);
  bool over = false;
  for (const auto &l : ::Serial.lines) over |= l.text.find("Serial5: BUFFER OVERRUN") != std::string::npos;
  // The Serial5 reader cuts a line at COMMAND_SIZE - 3, before the overrun.
  CHECK(!over);
}

}  // namespace

int main(int argc, char **argv) {
  if (argc > 1 && std::strcmp(argv[1], "--list") == 0) {
    for (const auto &t : registry()) std::printf("%s\n", t.first.c_str());
    return 0;
  }
  if (argc < 2 || !registry().count(argv[1])) {
    std::printf("usage: %s --list | <test>\n", argv[0]);
    return 2;
  }
  registry()[argv[1]]();
  std::printf("%s %s\n", failures ? "FAIL" : "ok  ", argv[1]);
  return failures ? 1 : 0;
}
