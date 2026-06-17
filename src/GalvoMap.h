#pragma once
//
// mm -> 16-bit galvo DAC count.
//
// Done in floating point so sub-millimetre coordinates are preserved: the
// XY2-100 path carries a 16-bit value over the field (X_MAX_POS_MM), i.e.
// ~3.8 um/count over 250 mm, far finer than the slicer's 0.1 mm hatch. The
// old code used Arduino's integer map(), which truncated the mm value to a
// whole number before scaling and collapsed every sub-mm coordinate onto the
// same count. Out-of-range inputs saturate to [0, 65535] instead of wrapping.
//
// The Teensy 4.0 has a hardware FPU and setGalvoPosition() already arrives
// here with double-precision interpolated coordinates, so this is not a hot
// path concern -- the galvo output is rate-limited by the ~10 us XY2-100
// frame, not by this arithmetic.
//
#include <stdint.h>

static inline uint16_t mmToGalvoCount(double mm, double maxMm, bool invert) {
    double pos = invert ? (maxMm - mm) : mm;
    double counts = (pos / maxMm) * 65535.0;
    if (counts < 0.0)     counts = 0.0;
    if (counts > 65535.0) counts = 65535.0;
    return (uint16_t)(counts + 0.5);
}
