// A host stand-in for the Arduino core. It carries only what
// src/SerialCMDReader.cpp uses: a Serial port fed from a string, the byte
// type and the core temperature. Thus the G-code parser compiles and runs on
// a PC, under the address sanitizer.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

typedef uint8_t byte;

class FakeSerial {
 public:
  std::string input;
  std::string output;
  size_t pos = 0;

  void feed(const std::string &text) { input += text; }
  int available() { return pos < input.size() ? 1 : 0; }
  int read() { return pos < input.size() ? (unsigned char)input[pos++] : -1; }
  void print(const char *s) { output += s; }
  void println(const char *s) { output += s; output += "\n"; }
  void println(double v) { output += std::to_string(v); output += "\n"; }
};

extern FakeSerial Serial;

// The Teensy core reads the die temperature. The parser prints it after each
// G line.
float tempmonGetTemp();
