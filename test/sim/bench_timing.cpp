// Laser timing benchmark. Streams a hatch layer through the simulated
// firmware, as osls1_klipper streams a slice, and measures what the beam does
// at the ends of each scan line. Prints one Markdown table row per scenario.
//
// Each scenario runs in its own process (fork), so the firmware's static
// state starts fresh every time, as after a power-up.
//
// Usage: bench_timing [--quick]
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <vector>

#include "sim.h"

namespace {

const int VECTORS = 40;
const int VECTORS_QUICK = 12;
// The window that keeps OPAL's 50-command queue fed, with room for the reply
// lines the host has not read yet.
const int STREAM_WINDOW = 40;

struct Scenario {
  int window;
  double rt_ms;
  double length_mm;
  double speed_mm_s;
  double tau_us;
};

void run_one(const Scenario &s, int vectors) {
  sim::boot();
  sim::RunConfig cfg;
  cfg.host.window = s.window;
  cfg.host.rt_ns = (uint64_t)(s.rt_ms * 1e6);
  sim::RunResult r = sim::stream(sim::hatch_layer(vectors, s.length_mm, s.speed_mm_s), cfg);
  sim::Metrics m = sim::measure(r, (uint64_t)(s.tau_us * 1e3), s.speed_mm_s);
  double scan_mm = vectors * s.length_mm;
  std::printf("| %s | %.2f | %.0f | %.0f | %.0f | %s | %.1f | %.0f | %.0f | %.0f | %.0f | %.3f | %.0f%% | %.0f%% |\n",
              s.window == 1 ? "line by line" : "window 40", s.rt_ms, s.length_mm, s.speed_mm_s,
              s.tau_us, r.done ? "yes" : "NO", m.layer_ms, m.late_off_mean_us, m.late_off_max_us,
              m.early_on_mean_us, m.dwell_total_us / vectors, m.jump_lit_mm / vectors,
              100.0 * m.lit_mm / scan_mm, 100.0 * m.dark_scan_mm / scan_mm);
  std::fflush(stdout);
}

}  // namespace

int main(int argc, char **argv) {
  bool quick = argc > 1 && std::strcmp(argv[1], "--quick") == 0;
  int vectors = quick ? VECTORS_QUICK : VECTORS;
  std::vector<Scenario> grid;
  const double rts[] = {0.25, 1.0, 2.0};
  const double lengths[] = {1, 2, 5, 20};
  const double speeds[] = {500, 2500};
  const double taus[] = {0, 150};
  for (double tau : taus)
    for (double speed : speeds)
      for (double len : lengths) {
        for (double rt : rts) grid.push_back({1, rt, len, speed, tau});
        grid.push_back({STREAM_WINDOW, 1.0, len, speed, tau});
      }
  std::printf("Hatch layer of %d lines, M5/G0/M3 S204/G1 per line, loop pass 2 us, PWM as the firmware sets it.\n\n", vectors);
  std::printf("| host | round trip ms | line mm | mm/s | mirror lag us | done | layer ms | late off mean us | late off max us | early on mean us | still and lit us per line | lit in jumps mm per line | lit while scanning | dark while scanning |\n");
  std::printf("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n");
  std::fflush(stdout);  // or every fork prints the header again
  int failures = 0;
  for (const auto &s : grid) {
    pid_t pid = fork();
    if (pid == 0) {
      run_one(s, vectors);
      _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) failures++;
  }
  return failures ? 1 : 0;
}
