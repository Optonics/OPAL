// Host tests of the G-code line parser (src/SerialCMDReader.*). They feed
// bytes through the fake Serial of stubs/Arduino.h and read the GCode records
// the parser queues. run.sh builds them with the address sanitizer. Thus a
// read past a buffer fails the run, even when the value happens to parse.
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>

#include <CircularBuffer.h>
#include "Helpers.h"
#include "SerialCMDReader.h"

FakeSerial Serial;
float tempmonGetTemp() { return 42.0f; }
uint64_t nanos() { return 0; }

static int failures = 0;

#define CHECK(cond)                                                    \
  do {                                                                 \
    if (!(cond)) {                                                     \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
      failures++;                                                      \
    }                                                                  \
  } while (0)

static bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

// The parser reads each number with strtod. The C library's strtod is not
// built with the sanitizer. Thus a read past the number buffer inside it goes
// unseen. This strtod replaces it in the test program: the sanitizer checks
// the strlen, which reads up to the terminator, and strtold does the parse.
extern "C" double strtod(const char *text, char **end) noexcept {
  volatile size_t length = std::strlen(text);
  (void)length;
  return (double)std::strtold(text, end);
}

// Every test gets a new parser and an empty port. The parser keeps its line
// buffer in a function static, so each line must end before the next test.
struct Rig {
  CircularBuffer<GCode, BUFFERSIZE> queue;
  SerialCMDReader reader{&queue};
  Rig() { Serial = FakeSerial(); }
  void send(const std::string &text) {
    Serial.feed(text);
    while (Serial.available()) reader.handleSerial();
  }
};

static void test_plain_move() {
  Rig rig;
  rig.send("G1 X10.5 Y20 F100\n");
  CHECK(rig.queue.size() == 1);
  GCode g = rig.queue.pop();
  CHECK(g.codeprefix == 'G' && g.code == 1);
  CHECK(near(g.x, 10.5) && near(g.y, 20) && near(g.f, 100));
  CHECK(g.s == MAX_VAL && g.z == MAX_VAL);
}

// The number buffer holds NUMBER_CHARS_MAX characters. A number of exactly
// that length needs a terminator after it.
static void test_number_of_max_length() {
  Rig rig;
  rig.send("G1 X123.456789 Y1\n");
  GCode g = rig.queue.pop();
  CHECK(near(g.x, 123.456789));
  CHECK(near(g.y, 1));
}

// A comment line is answered "ok" and queues nothing. The next line must not
// see the comment's characters: a Y in the comment is not a Y of the move.
static void test_comment_leaves_nothing_behind() {
  Rig rig;
  rig.send("; layer 7 Y99 F5 S200 comment\n");
  CHECK(rig.queue.size() == 0);
  CHECK(Serial.output == "ok\n");
  rig.send("G1 X10\n");
  CHECK(rig.queue.size() == 1);
  GCode g = rig.queue.pop();
  CHECK(near(g.x, 10));
  CHECK(g.y == MAX_VAL);
  CHECK(g.f == MAX_VAL);
  CHECK(g.s == MAX_VAL);
}

// Comment lines in a row must not add up. The scan length stays inside the
// line buffer, and a byte-sized index can never reach it.
static void test_many_comments_then_a_move() {
  Rig rig;
  for (int i = 0; i < 6; i++)
    rig.send("; " + std::string(80, 'c') + "\n");
  rig.send("G1 X1 Y2\n");
  CHECK(rig.queue.size() == 1);
  GCode g = rig.queue.pop();
  CHECK(near(g.x, 1) && near(g.y, 2));
}

// A line longer than the buffer is reported and cut. The line after it parses
// normally, and nothing reads past the buffer.
static void test_line_longer_than_buffer() {
  Rig rig;
  rig.send("G1 X5 Y6 " + std::string(COMMAND_SIZE + 60, '0') + "\n");
  CHECK(Serial.output.find("BUFFER OVERRUN") != std::string::npos);
  CHECK(rig.queue.size() == 1);
  rig.queue.pop();
  rig.send("G1 X3 Y4\n");
  CHECK(rig.queue.size() == 1);
  GCode g = rig.queue.pop();
  CHECK(near(g.x, 3) && near(g.y, 4));
}

struct NamedTest {
  const char *name;
  void (*run)();
};

static const NamedTest TESTS[] = {
    {"plain_move", test_plain_move},
    {"number_of_max_length", test_number_of_max_length},
    {"comment_leaves_nothing_behind", test_comment_leaves_nothing_behind},
    {"many_comments_then_a_move", test_many_comments_then_a_move},
    {"line_longer_than_buffer", test_line_longer_than_buffer},
};

// No argument runs every test. A test name runs that test alone. The
// sanitizer stops the process at the first bad read, so one test per process
// shows each result on its own.
int main(int argc, char **argv) {
  for (const NamedTest &t : TESTS)
    if (argc < 2 || std::string(argv[1]) == t.name) t.run();
  if (failures) {
    std::printf("%d check(s) failed\n", failures);
    return 1;
  }
  std::printf("all serial parser tests passed\n");
  return 0;
}
