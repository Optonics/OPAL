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
#ifndef CONFIGURATION_H

// Prints two lines for every M-code. The host discards them, but each one is
// USB traffic the host must read between two commands. On for debugging only.
//#define DEBUG_GCODES
// Prints the die temperature after every G line. Same cost; off by default.
//#define REPORT_TEMP_EACH_G

// Laser timing. The beam fires only while the mirror scans a G1. These
// delays are machine values to calibrate on a coupon or a scope; they start
// where they change little (docs/benchmarks).
//
// After a G1 starts, the beam waits this long: the mirror starts late.
#define LASER_ON_DELAY_US 0
// After a G1 ends, the beam stays on this long: the mirror ends late.
#define LASER_OFF_DELAY_US 0
// After a G0 or G28, the next command waits this long, plus the per-mm part
// for the jump length, so the mirror has arrived before a line starts.
#define JUMP_DELAY_MIN_US 100
#define JUMP_DELAY_PER_MM_US 4
// Serial bytes read per pass of loop(). One byte per pass made a 30-byte
// line take 30 passes; a whole line per pass keeps the queue fed.
#define SERIAL_BYTES_PER_PASS 64

#define CMDBUFFERSIZE 50 //Number of cashed GCodes
#define MBUFFERSIZE 20  //Buffersize for MCODES - number of consecutive M-Codes before another G-Code
#define DEFAULT_FEEDRATE 100

// The laser on the OSLS-1: a PWM diode, duty = power, 12-bit duty steps.
#define LASER_IS_DIODE
#define LASER_RESOLUTION 12
//#define LASER_G0_OFF_G1_ON

#define X_MAX 250 //mm
#define Y_MAX 250 //mm

#define INVERSE_X
//#define INVERSE_Y

#define CONFIGURATION_H
#endif