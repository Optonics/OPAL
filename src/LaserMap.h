#pragma once
//
// S-value -> laser PWM duty, for a simple PWM diode laser where duty = power.
//
// S in [0, maxS] maps linearly to duty [0, 2^bits - 1]. Crucially S=0 -> 0%
// duty (true off): the diode must be fully dark on laser-off jumps. There is
// no minimum-power floor (that was the Synrad CO2 tickle, wrong for a diode).
// Inputs are clamped so out-of-range S can neither overshoot nor wrap.
//
#include <stdint.h>

static inline uint16_t laserDutyFromS(double s, double maxS, int resolutionBits) {
    double maxDuty = (double)((1u << resolutionBits) - 1u);   // 12-bit -> 4095
    if (s < 0.0)  s = 0.0;
    if (s > maxS) s = maxS;
    double duty = (s / maxS) * maxDuty;
    if (duty > maxDuty) duty = maxDuty;
    return (uint16_t)(duty + 0.5);
}
