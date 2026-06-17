/*
  DiodeLaserCtrl.h - simple PWM diode-laser driver for OPAL on PJRC Teensy 4.x

  Part of OpenGalvo - OPAL Firmware

  Drives a laser whose control input is a plain PWM where duty cycle = power
  and 0% duty = off (active-high, logic-level). No tickle / warmup / power
  floor -- those are specific to the Synrad CO2 laser (see Synrad48Ctrl).

  OPAL Firmware is free software under the GNU GPL v3 or later.
*/

#pragma once

#ifdef __AVR__
#error "Sorry, this only works on 32 bit Teensy boards.  AVR isn't supported."
#endif

#if TEENSYDUINO < 121
#error "Minimum PJRC Teensyduino version 1.21 is required"
#endif

#ifndef DIODELASERCTRL_h
#define DIODELASERCTRL_h

#include <Arduino.h>
#include "LaserController.h"

// PWM carrier frequency for the diode modulation input. Our diode accepts
// 0-1 kHz active-high; set to the top of its range. Override before include.
#ifndef DIODE_PWM_FREQ_HZ
#define DIODE_PWM_FREQ_HZ 1000
#endif

// PWM duty resolution (bits). 12-bit => 0..4095 duty steps.
#ifndef DIODE_PWM_RESOLUTION_BITS
#define DIODE_PWM_RESOLUTION_BITS 12
#endif

class DiodeLaserCtrl : public LaserController {
  public:
    DiodeLaserCtrl();
    void begin(int PWM_OUT_Pin, int PSU_SSR_Pin);
    void stop();
    void update(uint16_t pwm);
    void update();
    bool isHalted();

  private:
    uint16_t laserPWM_OUT_Pin = 0;
    uint16_t laserPSU_SSR_Pin = 0;
    uint16_t lastPWM          = 0;
    bool     _isHalted        = true;
};

#endif
