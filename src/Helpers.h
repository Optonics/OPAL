/*
  Helpers.h - Helper functions to be used by OPAL FW on PJRC Teensy 4.x board

  Part of OpenGalvo - OPAL Firmware

  Copyright (c) 2020-2021 Daniel Olsson

  OPAL Firmware is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OPAL Firmware is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPAL Firmware.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#ifndef HELPERS_H
// configuration.h is the one place to set the field size, the feed rate, the
// laser type and the laser resolution. This file repeats none of them. The
// coordinate mapper in main.cpp reads X_MAX_POS_MM and Y_MAX_POS_MM. Both
// names come from configuration.h. Thus editing X_MAX there takes effect.
#include "configuration.h"

#define BUFFERSIZE 50
#define MAX_VAL 2147483630
#define X_MAX_POS_MM X_MAX
#define Y_MAX_POS_MM Y_MAX
#define LASER_MIN_PWM_PERCENT 10
#define LASER_MAX 255

  uint64_t nanos();

struct coordinate {
  double x = MAX_VAL;
  double y = MAX_VAL;
  double z = MAX_VAL;
};
  
  struct GCode {

  char codeprefix;
  int code; // Go = 0 , G28 = 28 etc. Only G0 / G1 Supported so far...
  double x;
  double y;
  double z;
  double e;

  double a;
  double b;
  double c;

  double i;
  double j;

  double p;

  double s; // Laser Power
  double f; // FeedRate
  double r; // Misc
  double t; // Misc
  double moveLengthNanos;
  char FWD_CMD[150];
};


void printDouble( double val, unsigned int precision);


  #define HELPERS_H
#endif
