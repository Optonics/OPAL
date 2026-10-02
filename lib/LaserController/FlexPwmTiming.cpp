/*
  FlexPwmTiming.cpp - see FlexPwmTiming.h.

  Register names from the Teensy 4 core (imxrt.h). Pin 6 and pin 9 are
  FlexPWM2 submodule 2, channels A and B (cores/teensy4/pwm.c).
*/
#include <Arduino.h>
#include "FlexPwmTiming.h"

static const int FLEXPWM2_SM2_PIN_A = 6;
static const int FLEXPWM2_SM2_PIN_B = 9;
static const int SUBMODULE = 2;

static bool isFlexPwm2Sm2(int pin) {
  return pin == FLEXPWM2_SM2_PIN_A || pin == FLEXPWM2_SM2_PIN_B;
}

void flexPwmLoadImmediately(int pin) {
  if (!isFlexPwm2Sm2(pin)) return;
  IMXRT_FLEXPWM2.SM[SUBMODULE].CTRL |= FLEXPWM_SMCTRL_LDMOD;
  // FRCEN lets a FORCE event re-initialise the counter (flexPwmRestartPeriod).
  IMXRT_FLEXPWM2.SM[SUBMODULE].CTRL2 |= FLEXPWM_SMCTRL2_FRCEN;
}

void flexPwmRestartPeriod(int pin) {
  if (!isFlexPwm2Sm2(pin)) return;
  // FORCE_SEL is 0 (the core's default): writing FORCE is a local force.
  IMXRT_FLEXPWM2.SM[SUBMODULE].CTRL2 |= FLEXPWM_SMCTRL2_FORCE;
}
