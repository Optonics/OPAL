// Native unit test for the laser S-value -> PWM duty mapping (LaserMap.h).
//
// For a PWM diode laser, "off" MUST be 0% duty. The old setLaserPower() used
// Arduino's integer map() with a LASER_MIN_PWM_PERCENT=10 floor (the Synrad
// CO2 tickle), so S=0 produced duty 10 -- a diode would idle ON during every
// laser-off jump. This pins: S=0 -> 0, S=max -> full-scale, linear, clamped.
//
//   g++ -std=c++11 -Wall test/native/test_laser_map.cpp -o /tmp/test_laser_map && /tmp/test_laser_map

#include "../../src/LaserMap.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

static int g_failures = 0;
static void check_eq(const char *expr, long actual, long expected) {
    if (actual != expected) {
        std::printf("FAIL  %-40s => %ld, expected %ld\n", expr, actual, expected);
        ++g_failures;
    } else {
        std::printf("ok    %-40s => %ld\n", expr, actual);
    }
}
#define CHECK_EQ(expr, expected) check_eq(#expr, (long)(expr), (long)(expected))

int main() {
    const double MAXS = 255.0;   // LASER_MAX (gcode S range)
    const int    RES  = 12;      // LASER_RESOLUTION -> 12-bit, full-scale 4095

    // OFF must be a true zero -- the whole point of dropping the Synrad floor.
    CHECK_EQ(laserDutyFromS(0.0,   MAXS, RES), 0);
    // Full power -> full-scale duty.
    CHECK_EQ(laserDutyFromS(255.0, MAXS, RES), 4095);
    // The slicer's fixed S204 -> ~80% duty.
    CHECK_EQ(laserDutyFromS(204.0, MAXS, RES), 3276);
    // Half power.
    CHECK_EQ(laserDutyFromS(127.5, MAXS, RES), 2048);
    // Out-of-range clamps (no wrap, no overshoot).
    CHECK_EQ(laserDutyFromS(300.0, MAXS, RES), 4095);
    CHECK_EQ(laserDutyFromS(-5.0,  MAXS, RES), 0);

    std::printf("\n%s (%d failure%s)\n",
                g_failures ? "TESTS FAILED" : "ALL TESTS PASSED",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
