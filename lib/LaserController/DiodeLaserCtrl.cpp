/*
  DiodeLaserCtrl.cpp - simple PWM diode-laser driver for OPAL on Teensy 4.x

  Part of OpenGalvo - OPAL Firmware. GNU GPL v3 or later.
*/

#include "DiodeLaserCtrl.h"

DiodeLaserCtrl::DiodeLaserCtrl() {}

void DiodeLaserCtrl::begin(int PWM_OUT_Pin, int PSU_SSR_Pin) {
  laserPWM_OUT_Pin = PWM_OUT_Pin;
  laserPSU_SSR_Pin = PSU_SSR_Pin;

  // Force the modulation output to a known-off state before anything else.
  pinMode(laserPWM_OUT_Pin, OUTPUT);
  analogWriteResolution(DIODE_PWM_RESOLUTION_BITS);
  analogWriteFrequency(laserPWM_OUT_Pin, DIODE_PWM_FREQ_HZ);
  analogWrite(laserPWM_OUT_Pin, 0);   // 0% duty = laser off
  lastPWM = 0;

  _isHalted = false;
  // The PSU/enable SSR (laserPSU_SSR_Pin) is owned by the M80/M81 handler in
  // MotionMGR; begin() leaves it in whatever state the caller set.
}

void DiodeLaserCtrl::stop() {
  analogWrite(laserPWM_OUT_Pin, 0);        // laser off
  lastPWM = 0;
  digitalWrite(laserPSU_SSR_Pin, LOW);     // drop PSU / enable
  _isHalted = true;
}

void DiodeLaserCtrl::update(uint16_t pwm) {
  lastPWM = pwm;
  analogWrite(laserPWM_OUT_Pin, pwm);      // duty = power; 0 = off
}

void DiodeLaserCtrl::update() {
  analogWrite(laserPWM_OUT_Pin, lastPWM);  // re-apply (unused by current flow)
}

bool DiodeLaserCtrl::isHalted() {
  return _isHalted;
}
