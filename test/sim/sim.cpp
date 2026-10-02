// The simulator harness, the replay of the laser pin and the mirrors, and the
// timing measurements. See sim.h for what is modelled.
#include "sim.h"

#include <algorithm>
#include <cmath>
#include <deque>

// LASER_PWM_OUT_PIN and X_MAX of the firmware under test (end of file).
extern const int SIM_LASER_PIN;
extern const double SIM_FIELD_MM;

namespace sim {

static const double COUNTS_FULL_SCALE = 65535.0;
static const double NS_PER_US = 1000.0;
static const double NS_PER_MS = 1000000.0;
static const double NS_PER_S = 1e9;

void boot() {
  reset();
  setup();
}

RunResult stream(const std::vector<std::string> &lines, const RunConfig &cfg) {
  std::vector<std::string> all = lines;
  all.push_back("M400");  // opal_link.wait_done()
  RunResult r;
  r.start_ns = cfg.host_start_ns;
  std::deque<uint64_t> releases(cfg.host.window, cfg.host_start_ns);
  uint32_t rng = cfg.host.seed;
  auto round_trip = [&]() {
    rng = rng * 1664525u + 1013904223u;  // LCG: the same run every time
    double u = (rng >> 8) / 16777216.0;  // 0..1
    return (uint64_t)(cfg.host.rt_ns * (1.0 + cfg.host.jitter * (2 * u - 1)));
  };
  size_t next = 0;
  size_t scanned = Serial.lines.size();
  while (now_ns < cfg.limit_ns) {
    while (scanned < Serial.lines.size()) {
      const SimSerial::Line &line = Serial.lines[scanned++];
      if (line.text == "ok") releases.push_back(line.t + round_trip());
      if (line.text == "done" && next == all.size()) {
        r.done = true;
        r.end_ns = line.t;
      }
    }
    if (r.done) break;
    while (next < all.size() && !releases.empty() && releases.front() <= now_ns) {
      Serial.deliver(all[next++] + "\n", releases.front());
      releases.pop_front();
    }
    loop();
    r.loops++;
    now_ns += cfg.loop_ns;
  }
  if (!r.done) r.end_ns = now_ns;
  return r;
}

void idle(uint64_t ns, uint64_t loop_ns) {
  uint64_t until = now_ns + ns;
  while (now_ns < until) {
    loop();
    now_ns += loop_ns;
  }
}

void send_now(const std::string &text, uint64_t loop_ns) {
  Serial.deliver(text, now_ns);
  // A full command queue stops the reader; the bound keeps a stuck run finite.
  for (int i = 0; i < 1000000 && !Serial.in.empty(); ++i) {
    loop();
    now_ns += loop_ns;
  }
}

int count_lines(const std::string &text) {
  int n = 0;
  for (const auto &line : Serial.lines) n += (line.text == text);
  return n;
}

Replay::Replay(int laser_pin, double field_mm, uint64_t mirror_tau_ns)
    : field_mm_(field_mm), tau_ns_(mirror_tau_ns) {
  if (analog_frequency_hz > 0) period_ns_ = (uint64_t)std::llround(NS_PER_S / analog_frequency_hz);
  period_origin_ns_ = analog_frequency_set_ns;
  duty_max_ = (1 << analog_resolution_bits) - 1;
  // The duty latches at the next period boundary, or at once after
  // flexPwmLoadImmediately. A restart starts a new period now.
  bool immediate = false;
  uint64_t origin = period_origin_ns_;
  for (const auto &e : pin_events) {
    if (e.pin != laser_pin) continue;
    if (e.op == PinOp::LoadImmediate) {
      immediate = true;
    } else if (e.op == PinOp::Restart) {
      origin = e.t;
      changes_.push_back({e.t, 4, 0});
    } else if (e.op == PinOp::Mode) {
      changes_.push_back({e.t, 0, 0});
    } else if (e.op == PinOp::Digital) {
      changes_.push_back({e.t, 1, e.value});
    } else {
      changes_.push_back({e.t, 3, 0});
      uint64_t latch = e.t;
      if (!immediate && period_ns_ > 0 && e.t > origin) {
        uint64_t since = e.t - origin;
        latch = origin + ((since + period_ns_ - 1) / period_ns_) * period_ns_;
      }
      changes_.push_back({latch, 2, e.value});
    }
  }
  std::stable_sort(changes_.begin(), changes_.end(),
                   [](const Change &a, const Change &b) { return a.t < b.t; });
  State s;
  s.origin = period_origin_ns_;
  for (const auto &c : changes_) {
    if (c.kind == 4) s.origin = c.t;
    if (c.kind == 0) s.gpio = true;
    if (c.kind == 1) s.level = c.value;
    if (c.kind == 2) s.duty = c.value;
    if (c.kind == 3) s.gpio = false;
    snapshots_.push_back({c.t, s});
  }
}

Replay::State Replay::state_at(uint64_t t) const {
  auto it = std::upper_bound(snapshots_.begin(), snapshots_.end(), t,
                             [](uint64_t v, const std::pair<uint64_t, State> &p) { return v < p.first; });
  if (it == snapshots_.begin()) return State();
  return std::prev(it)->second;
}

bool Replay::enabled(uint64_t t) const {
  State s = state_at(t);
  return s.gpio ? s.level != 0 : s.duty > 0;
}

bool Replay::high(uint64_t t) const {
  State s = state_at(t);
  if (s.gpio) return s.level != 0;
  if (s.duty <= 0) return false;
  if (period_ns_ == 0 || s.duty >= duty_max_) return true;
  uint64_t phase = (t - s.origin) % period_ns_;
  return phase < (uint64_t)((double)s.duty / (duty_max_ + 1) * period_ns_);
}

int Replay::duty(uint64_t t) const { return state_at(t).duty; }

void Replay::commanded_mm(uint64_t t, double *x, double *y) const {
  const auto &ev = galvo_events;
  auto it = std::upper_bound(ev.begin(), ev.end(), t,
                             [](uint64_t v, const GalvoEvent &e) { return v < e.t; });
  if (it == ev.begin()) {
    *x = *y = 0;
    return;
  }
  --it;
  *x = it->x / COUNTS_FULL_SCALE * field_mm_;
  *y = it->y / COUNTS_FULL_SCALE * field_mm_;
}

void Replay::mirror_mm(uint64_t t, double *x, double *y) {
  double cx, cy;
  commanded_mm(t, &cx, &cy);
  if (!mirror_started_ || tau_ns_ == 0) {
    mx_ = cx;
    my_ = cy;
  } else {
    double k = 1.0 - std::exp(-(double)(t - mt_) / (double)tau_ns_);
    mx_ += (cx - mx_) * k;
    my_ += (cy - my_) * k;
  }
  mirror_started_ = true;
  mt_ = t;
  *x = mx_;
  *y = my_;
}

Metrics measure(const RunResult &run, uint64_t mirror_tau_ns, double scan_mm_s,
                double sample_ns, double window_ns) {
  Replay rp(SIM_LASER_PIN, SIM_FIELD_MM, mirror_tau_ns);
  Metrics m;
  const double still = 0.2 * scan_mm_s;
  const double jump = 1.5 * scan_mm_s;
  const size_t lag = std::max<size_t>(1, (size_t)(window_ns / sample_ns));
  std::deque<std::pair<double, double>> hist;
  double px = 0, py = 0;
  bool have_prev = false, was_on = false, moved_in_run = false;
  uint64_t rise = 0, first_move = 0, last_move = 0;
  double late_sum = 0, early_sum = 0;
  for (uint64_t t = run.start_ns; t <= run.end_ns; t += (uint64_t)sample_ns) {
    double x, y;
    rp.mirror_mm(t, &x, &y);
    hist.emplace_back(x, y);
    if (hist.size() > lag + 1) hist.pop_front();
    double speed = std::hypot(x - hist.front().first, y - hist.front().second) /
                   (window_ns / NS_PER_S);
    double step = have_prev ? std::hypot(x - px, y - py) : 0;
    px = x;
    py = y;
    have_prev = true;
    bool on = rp.enabled(t);
    bool hi = rp.high(t);
    bool moving = speed >= still;
    if (on && !moving) m.dwell_total_us += sample_ns / NS_PER_US;
    if (hi && speed > jump) m.jump_lit_mm += step;
    if (hi && moving && speed <= jump) m.lit_mm += step;
    if (on && !hi && moving && speed <= jump) m.dark_scan_mm += step;
    if (on && !was_on) {
      rise = t;
      moved_in_run = false;
    }
    if (on && moving && speed <= jump) {
      if (!moved_in_run) first_move = t;
      moved_in_run = true;
      last_move = t;
    }
    if (!on && was_on && moved_in_run) {
      double late = (t - last_move) / NS_PER_US;
      double early = (first_move - rise) / NS_PER_US;
      late_sum += late;
      early_sum += early;
      m.late_off_max_us = std::max(m.late_off_max_us, late);
      m.early_on_max_us = std::max(m.early_on_max_us, early);
      m.lit_runs++;
    }
    was_on = on;
  }
  if (m.lit_runs) {
    m.late_off_mean_us = late_sum / m.lit_runs;
    m.early_on_mean_us = early_sum / m.lit_runs;
  }
  m.layer_ms = (run.end_ns - run.start_ns) / NS_PER_MS;
  return m;
}

std::vector<std::string> hatch_layer(int vectors, double length_mm, double speed_mm_s, int s,
                                     double spacing_mm) {
  const double x0 = 100.0, y0 = 100.0;
  const int jump_speed = 4000;
  std::vector<std::string> out;
  char buf[96];
  for (int i = 0; i < vectors; ++i) {
    double y = y0 + i * spacing_mm;
    double sx = x0;
    double ex = x0 + length_mm;
    out.push_back("M5");
    std::snprintf(buf, sizeof buf, "G0 F%d X%.3f Y%.3f", jump_speed, sx, y);
    out.push_back(buf);
    std::snprintf(buf, sizeof buf, "M3 S%d", s);
    out.push_back(buf);
    std::snprintf(buf, sizeof buf, "G1 F%.0f X%.3f Y%.3f", speed_mm_s, ex, y);
    out.push_back(buf);
  }
  out.push_back("M5");
  return out;
}

}  // namespace sim

// The firmware's laser pin and field, read from its own headers.
#include "Pins.h"
#include "configuration.h"
extern const int SIM_LASER_PIN = LASER_PWM_OUT_PIN;
extern const double SIM_FIELD_MM = X_MAX;
