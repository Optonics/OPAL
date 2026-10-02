/*
  FlexPwmTiming.h - make a PWM duty change reach the laser pin at once.

  Part of OpenGalvo - OPAL Firmware. GNU GPL v3 or later.

  The Teensy 4 core writes a new duty into a buffered compare register and
  the FlexPWM loads it at the start of the next PWM period. At the diode's
  1 kHz that is up to 1 ms late: a beam told to go dark keeps firing while
  the mirror jumps, and a beam told to light starts up to 1 ms into the scan
  line (docs/benchmarks/laser-timing-baseline.md).

  flexPwmLoadImmediately() sets the submodule's LDMOD bit: a written duty is
  loaded as soon as the core sets LDOK. analogWriteFrequency() rewrites the
  control register and clears it, so call this after it.

  flexPwmRestartPeriod() starts a new PWM period now (a FORCE event with
  FRCEN set re-initialises the counter). A beam lit from dark then starts its
  high phase at once instead of at the next period.

  Only the FlexPWM2 submodule 2 pins (6 and 9) are handled: pin 6 is
  LASER_PWM_OUT_PIN. Any other pin is left alone and keeps the core's
  buffered behaviour.
*/
#pragma once

void flexPwmLoadImmediately(int pin);
void flexPwmRestartPeriod(int pin);
