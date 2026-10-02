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

int last_digital(int pin) {
  for (auto it = sim::pin_events.rbegin(); it != sim::pin_events.rend(); ++it)
    if (it->pin == pin && it->op == sim::PinOp::Digital) return it->value;
  return -1;
}

sim::Replay replay() { return sim::Replay(LASER_PWM_OUT_PIN, X_MAX); }
bool beam_now() { return replay().enabled(sim::now_ns); }
double pos_x_mm() { return sim::galvo_events.back().x / FULL * X_MAX; }
double pos_y_mm() { return sim::galvo_events.back().y / FULL * Y_MAX; }
bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }
// Sends a line and runs until the queue has executed it.
void run_line(const std::string &line, uint64_t after_ns = 1 * MS) {
  sim::send_now(line + "\n");
  sim::idle(after_ns);
}

// Arms with `arm`, then scans a 1 mm G1 at 100 mm/s (10 ms) and returns the
// duty the pin carries half way. The beam lights only while it scans.
int duty_while_scanning(const std::string &arm) {
  sim::send_now(arm + "\n");
  sim::idle(1 * MS);
  char line[48];
  std::snprintf(line, sizeof line, "G1 F100 X%.3f\n", pos_x_mm() + 1.0);
  sim::send_now(line);
  sim::idle(5 * MS);
  int duty = beam_now() ? replay().duty(sim::now_ns) : 0;
  sim::idle(10 * MS);
  return duty;
}

// --- Start-up ---------------------------------------------------------------

TEST(boot_leaves_laser_off_at_1khz_12bit) {
  sim::boot();
  CHECK(!beam_now());
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

TEST(m3_arms_the_beam_and_m5_disarms_it) {
  sim::boot();
  CHECK(duty_while_scanning("M3 S204") == DUTY_S204);
  CHECK(!beam_now());                      // dark again after the line
  CHECK(duty_while_scanning("M5") == 0);
}

TEST(m4_is_m3) {
  sim::boot();
  CHECK(duty_while_scanning("M4 S255") == 4095);
}

TEST(m3_without_s_keeps_the_last_power) {
  sim::boot();
  duty_while_scanning("M3 S204");
  run_line("M5");
  CHECK(duty_while_scanning("M3") == DUTY_S204);
}

TEST(s_out_of_range_clamps_negative) {
  sim::boot();
  CHECK(duty_while_scanning("M3 S300") == 4095);
  CHECK(duty_while_scanning("M3 S-5") == 0);
  CHECK(duty_while_scanning("M3 S0") == 0);
}

TEST(g1_with_s_changes_the_power) {
  sim::boot();
  run_line("M3 S100");
  sim::send_now("G1 F100 X1 S200\n");
  sim::idle(5 * MS);
  CHECK(replay().duty(sim::now_ns) == laserDutyFromS(200, 255, LASER_RESOLUTION));
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
  CHECK(sim::count_lines("ok") == 3);
  CHECK(duty_while_scanning("G90") == DUTY_S204);  // still armed
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
  sim::idle(20 * MS);                       // 60 jumps, each settles 0.1 ms
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

TEST(stream_line_by_line_finishes_a_layer) {
  sim::boot();
  sim::RunConfig cfg;
  sim::RunResult r = sim::stream(sim::hatch_layer(10, 2, 500), cfg);
  CHECK(r.done);
  CHECK(!beam_now());                      // the layer ends dark
}

// --- Dead-man ------------------------------------------------------------------

TEST(dead_man_disarms_after_a_second_of_host_silence) {
  sim::boot();
  run_line("M3 S204", 500 * MS);           // half a second: still armed
  CHECK(duty_while_scanning("G90") == DUTY_S204);
  run_line("M3 S204", 1100 * MS);          // over the 1 s timeout
  CHECK(duty_while_scanning("G90") == 0);
  CHECK(duty_while_scanning("M3 S204") == DUTY_S204);  // M3 arms it again
}

TEST(dead_man_waits_while_a_move_runs) {
  sim::boot();
  run_line("M3 S204");
  sim::send_now("G1 F10 X20\n");           // 2 s with no serial traffic
  sim::idle(1900 * MS, 20 * US);
  CHECK(beam_now());
}

TEST(m80_m81_switch_the_laser_supply) {
  sim::boot();
  run_line("M81");
  CHECK(last_digital(LASER_SSR_OUT_PIN) == 0);
  CHECK(!beam_now());
  run_line("M80", 300 * MS);               // waits 250 ms, then restarts the driver
  CHECK(last_digital(LASER_SSR_OUT_PIN) == 1);
  CHECK(duty_while_scanning("M3 S204") == DUTY_S204);
  run_line("M80");                          // already on: nothing to restart
  CHECK(last_digital(LASER_SSR_OUT_PIN) == 1);
  // Each reply is a whole line: the host reads "ok" exactly.
  CHECK(sim::count_lines("ok") == 5);    // M81, M80, M3, G1, M80
  for (const auto &l : ::Serial.lines) CHECK(l.text.find("Set 1") == std::string::npos);
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


// --- Laser timing (feature/diode-laser-timing) -----------------------------------
//
// The beam fires only while the mirror scans, and goes dark at once. Each of
// these failed on the firmware before the timing fixes: see
// docs/benchmarks/laser-timing-baseline.md for the measurements.

#ifndef JUMP_DELAY_MIN_US
#define JUMP_DELAY_MIN_US 100
#endif
#ifndef JUMP_DELAY_PER_MM_US
#define JUMP_DELAY_PER_MM_US 4
#endif
#ifndef LASER_ON_DELAY_US
#define LASER_ON_DELAY_US 0
#endif
#ifndef LASER_OFF_DELAY_US
#define LASER_OFF_DELAY_US 0
#endif


// When the beam (the pin, not the register) first goes from dark to enabled
// at or after t0, and back.
uint64_t beam_rise_after(uint64_t t0, uint64_t step = 1 * US, uint64_t until = 50 * MS) {
  sim::Replay rp = replay();
  for (uint64_t t = t0; t < t0 + until; t += step)
    if (rp.enabled(t)) return t;
  return UINT64_MAX;
}
uint64_t beam_fall_after(uint64_t t0, uint64_t step = 1 * US, uint64_t until = 50 * MS) {
  sim::Replay rp = replay();
  for (uint64_t t = t0; t < t0 + until; t += step)
    if (!rp.enabled(t)) return t;
  return UINT64_MAX;
}

TEST(timing_beam_goes_dark_within_10_us_of_the_line_end) {
  sim::boot();
  sim::send_now("M3 S204\nG1 F500 X1\nM5\n");  // 1 mm at 500 mm/s: 2 ms
  sim::idle(10 * MS);
  uint64_t end_of_line = UINT64_MAX;
  for (const auto &e : sim::galvo_events)
    if (near(e.x / FULL * X_MAX, 1.0, 1e-3)) { end_of_line = e.t; break; }
  CHECK(end_of_line != UINT64_MAX);
  uint64_t dark = beam_fall_after(end_of_line);
  CHECK(dark != UINT64_MAX);
  CHECK(dark - end_of_line <= LASER_OFF_DELAY_US * US + 10 * US);
}

TEST(timing_beam_lights_within_10_us_of_the_line_start) {
  sim::boot();
  sim::send_now("M3 S204\nG1 F500 X1\n");
  sim::idle(10 * MS);
  uint64_t first_move = UINT64_MAX;
  for (const auto &e : sim::galvo_events)
    if (e.x > 0) { first_move = e.t; break; }
  CHECK(first_move != UINT64_MAX);
  uint64_t lit = beam_rise_after(0);
  CHECK(lit != UINT64_MAX);
  CHECK(lit + 10 * US >= first_move);        // not before the mirror moves
  CHECK(lit <= first_move + LASER_ON_DELAY_US * US + 10 * US);
}

TEST(timing_m3_alone_does_not_light_a_standing_mirror) {
  sim::boot();
  run_line("G0 X50 Y50");
  run_line("M3 S204", 5 * MS);
  CHECK(beam_rise_after(0) == UINT64_MAX);
}

TEST(timing_g0_turns_the_beam_off) {
  sim::boot();
  sim::send_now("M3 S204\nG1 F500 X1\nG0 X50\nG1 F500 X51\n");
  sim::idle(20 * MS);
  // The G1 after the G0 has no M3 of its own: it runs dark.
  uint64_t jump = UINT64_MAX;
  for (const auto &e : sim::galvo_events)
    if (near(e.x / FULL * X_MAX, 50.0, 1e-3)) { jump = e.t; break; }
  CHECK(jump != UINT64_MAX);
  CHECK(!replay().enabled(jump));
  CHECK(beam_rise_after(jump) == UINT64_MAX);
}

TEST(timing_a_contour_stays_lit_through_its_corners) {
  sim::boot();
  sim::send_now("G0 X10 Y10\nM3 S204\nG1 F500 X20 Y10\nG1 F500 X20 Y20\nG1 F500 X10 Y20\nG1 F500 X10 Y10\nM5\n");
  sim::idle(200 * MS);
  uint64_t on = beam_rise_after(0);
  CHECK(on != UINT64_MAX);
  uint64_t off = beam_fall_after(on, 1 * US, 200 * MS);
  // 40 mm at 500 mm/s: 80 ms lit in one piece.
  CHECK(off != UINT64_MAX && off - on >= 79 * MS);
}

TEST(timing_the_next_line_waits_for_the_jump_to_settle) {
  sim::boot();
  run_line("G0 X10");
  sim::send_now("G0 X210\nM3 S204\nG1 F500 X211\n");
  uint64_t jump = sim::now_ns;
  sim::idle(20 * MS);
  uint64_t start = UINT64_MAX;
  for (const auto &e : sim::galvo_events)
    if (e.x / FULL * X_MAX > 210.0 + 1e-3) { start = e.t; break; }
  CHECK(start != UINT64_MAX);
  const uint64_t settle = (JUMP_DELAY_MIN_US + 200 * JUMP_DELAY_PER_MM_US) * US;
  CHECK(start - jump >= settle);
}

TEST(timing_a_line_is_read_in_one_pass) {
  sim::boot();
  ::Serial.deliver("G0 X10\n", sim::now_ns);
  int passes = 0;
  while (passes < 50 && !(sim::galvo_events.size() && near(pos_x_mm(), 10, 0.01))) {
    loop();
    sim::now_ns += 2 * US;
    passes++;
  }
  CHECK(passes <= 3);                         // read, queue, move
}

TEST(timing_a_g_line_answers_only_ok) {
  sim::boot();
  size_t before = ::Serial.lines.size();
  run_line("G0 X1");
  run_line("M3 S10");
  run_line("M5");
  CHECK(::Serial.lines.size() - before == 3);   // three oks, nothing else
}

TEST(timing_line_by_line_host_leaves_no_lit_dwell) {
  sim::boot();
  sim::RunConfig cfg;                       // window 1, 1 ms round trip
  sim::RunResult r = sim::stream(sim::hatch_layer(20, 1, 2500), cfg);
  CHECK(r.done);
  sim::Metrics m = sim::measure(r, 0, 2500);
  CHECK(m.dwell_total_us / 20 <= LASER_OFF_DELAY_US + LASER_ON_DELAY_US + 30);
  CHECK(m.late_off_mean_us <= LASER_OFF_DELAY_US + 30);
}

TEST(timing_full_window_host_draws_no_streak_in_the_jumps) {
  sim::boot();
  sim::RunConfig cfg;
  cfg.host.window = 40;
  sim::RunResult r = sim::stream(sim::hatch_layer(20, 20, 2500), cfg);
  CHECK(r.done);
  sim::Metrics m = sim::measure(r, 0, 2500);
  CHECK(m.jump_lit_mm < 0.05 * 20);           // under 0.05 mm per jump
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
