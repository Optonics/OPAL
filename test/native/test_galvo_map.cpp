// Native unit test for the mm -> 16-bit galvo DAC-count conversion.
//
// This pins the fix for the integer-map() resolution bug: the old code did
// `map((double)x, 0, X_MAX_POS_MM, 0, 65535)`, where Arduino's integer
// `long map(long,...)` truncated the millimetre value to a whole number
// BEFORE scaling -- collapsing everything within a 1 mm band onto the same
// DAC count. The slicer's 0.1 mm hatch spacing was therefore quantised away.
//
// Build & run natively (from the OPAL repo root), no hardware needed:
//   g++ -std=c++11 -Wall test/native/test_galvo_map.cpp -o /tmp/test_galvo_map && /tmp/test_galvo_map
//
// Expected: RED while src/GalvoMap.h still uses integer truncation,
//           GREEN once mmToGalvoCount() does the scaling in floating point.

#include "../../src/GalvoMap.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

static int g_failures = 0;

static void check_eq(const char *expr, long actual, long expected) {
    if (actual != expected) {
        std::printf("FAIL  %-44s => %ld, expected %ld\n", expr, actual, expected);
        ++g_failures;
    } else {
        std::printf("ok    %-44s => %ld\n", expr, actual);
    }
}

#define CHECK_EQ(expr, expected) check_eq(#expr, (long)(expr), (long)(expected))

int main() {
    const double MAX = 250.0;   // X_MAX_POS_MM / Y_MAX_POS_MM

    // --- Endpoints and centre map to the full 16-bit range ---
    CHECK_EQ(mmToGalvoCount(0.0,   MAX, false), 0);
    CHECK_EQ(mmToGalvoCount(250.0, MAX, false), 65535);
    CHECK_EQ(mmToGalvoCount(125.0, MAX, false), 32768);   // 0.5 * 65535 = 32767.5 -> 32768

    // --- THE bug: sub-millimetre coordinates must be preserved ---
    // 140.47 / 250 * 65535 = 36822.8 -> 36823 (integer map gave 36699 for any 140.xx)
    CHECK_EQ(mmToGalvoCount(140.47, MAX, false), 36823);

    // Two coordinates inside the same whole millimetre must land on different counts.
    if (mmToGalvoCount(140.1, MAX, false) == mmToGalvoCount(140.9, MAX, false)) {
        std::printf("FAIL  sub-mm coordinates collapse to the same DAC count\n");
        ++g_failures;
    } else {
        std::printf("ok    sub-mm coordinates are distinguishable\n");
    }

    // A 0.1 mm hatch step must actually move the galvo (250mm/65535 ~= 3.8um/count,
    // so 0.1 mm ~= 26 counts).
    {
        int delta = (int)mmToGalvoCount(100.1, MAX, false)
                  - (int)mmToGalvoCount(100.0, MAX, false);
        if (delta < 20) {
            std::printf("FAIL  0.1 mm step moved only %d counts (expected ~26)\n", delta);
            ++g_failures;
        } else {
            std::printf("ok    0.1 mm step moves %d counts\n", delta);
        }
    }

    // --- Axis inversion still works ---
    CHECK_EQ(mmToGalvoCount(0.0,   MAX, true), 65535);
    CHECK_EQ(mmToGalvoCount(250.0, MAX, true), 0);

    // --- Out-of-range inputs saturate instead of wrapping to garbage ---
    CHECK_EQ(mmToGalvoCount(-10.0, MAX, false), 0);
    CHECK_EQ(mmToGalvoCount(300.0, MAX, false), 65535);

    std::printf("\n%s (%d failure%s)\n",
                g_failures ? "TESTS FAILED" : "ALL TESTS PASSED",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
