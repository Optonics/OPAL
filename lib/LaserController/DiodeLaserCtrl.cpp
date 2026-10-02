/*
  DiodeLaserCtrl.cpp - simple PWM diode-laser driver for OPAL on Teensy 4.x

  Part of OpenGalvo - OPAL Firmware. GNU GPL v3 or later.
*/

#include "DiodeLaserCtrl.h"
#include "FlexPwmTiming.h"

DiodeLaserCtrl::DiodeLaserCtrl() {}

void DiodeLaserCtrl::begin(int PWM_OUT_Pin, int PSU_SSR_Pin) {
  laserPWM_OUT_Pin = PWM_OUT_Pin;
  laserPSU_SSR_Pin = PSU_SSR_Pin;

  analogWriteResolution(DIODE_PWM_RESOLUTION_BITS);
  analogWriteFrequency(laserPWM_OUT_Pin, DIODE_PWM_FREQ_HZ);
  // After the frequency: analogWriteFrequency() rewrites the control register.
  flexPwmLoadImmediately(laserPWM_OUT_Pin);
  dark();

  _isHalted = false;
  // The PSU/enable SSR (laserPSU_SSR_Pin) is owned by the M80/M81 handler in
  // MotionMGR; begin() leaves it in whatever state the caller set.
}

// The pin as a plain GPIO driven low. Unlike a 0 % duty, which the PWM
// would take only at the end of its current period, this is dark at once.
void DiodeLaserCtrl::dark() {
  pinMode(laserPWM_OUT_Pin, OUTPUT);
  digitalWrite(laserPWM_OUT_Pin, LOW);
  lastPWM = 0;
}

void DiodeLaserCtrl::stop() {
  dark();
  digitalWrite(laserPSU_SSR_Pin, LOW);     // drop PSU / enable
  _isHalted = true;
}

// duty = power; 0 = off. MotionMGR calls this on every change of the beam.
void DiodeLaserCtrl::update(uint16_t pwm) {
  if (pwm == lastPWM) return;
  if (pwm == 0) {
    dark();
    return;
  }
  bool wasDark = lastPWM == 0;
  analogWrite(laserPWM_OUT_Pin, pwm);      // back on the PWM; loads at once
  if (wasDark) flexPwmRestartPeriod(laserPWM_OUT_Pin);  // high phase starts now
  lastPWM = pwm;
}

void DiodeLaserCtrl::update() {
  analogWrite(laserPWM_OUT_Pin, lastPWM);  // re-apply (unused by current flow)
}

bool DiodeLaserCtrl::isHalted() {
  return _isHalted;
}
