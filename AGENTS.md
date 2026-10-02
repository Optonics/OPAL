# AGENTS.md - OPAL galvo firmware

Persistent memory for this repo. Read the workspace `../AGENTS.md` first. It holds the cross-repo rules. Read this file after that.

## The protocol

1. **Read this whole file before doing anything else.** It is short on purpose. If reading it feels expensive, prune it.
2. **Before ending a session, update it.** Ask what you learned that the next person, who may be you months from now with no memory of today, would otherwise have to rediscover. Write that down. Skip anything derivable from the code, git history, or README. Those already persist.
3. **Every entry is dated** (`YYYY-MM-DD`) and states *why*, not just *what*. Undated or unexplained entries are the first candidates for pruning.
4. **Convert relative time to absolute.** Never write "last week" or "soon". Write the date or the version number.
5. **Overwrite, don't accumulate.** "Current state" and "Open threads" describe *now*. Rewrite them each time they change. "Decisions" and "Gotchas" are append-mostly. Delete entries that stopped being true, and say so in the commit message.
6. **Commit this file with the work it describes.** Thus the memory and the code never drift apart.
7. Formatting: plain hyphens only (no em or en dashes). In Markdown, one line per paragraph (no hard wrapping). Code comments wrap at the line width of their file. Use named constants over repeated magic strings in any code you write.
8. Docs and comments are written in simplified technical English: short declarative sentences, one idea per sentence, "Thus ..." to state a consequence. Match it.

## Project facts (stable)

- OPAL is firmware that turns G-code into the XY2-100 protocol that drives digital galvos, plus peripheral control. Upstream is the OpenGalvo project. This repo is Optonics's copy of it. Target is a Teensy 4.0 (not a 3.6) with an SN75174N on pins 22, 19, 17, 14. PlatformIO, `env:teensy40`, Arduino framework.
- Upstream calls it **pre-alpha and not ready for mainstream use**, with explicit warnings that the board, the galvo, the laser or something nearby can be destroyed. Treat any change as safety-relevant.
- `src/configuration.h` is the whole tunable surface and it is short: `X_MAX`/`Y_MAX` (both 250 mm), `LASER_IS_SYNRAD`, `LASER_RESOLUTION 12`, `LASER_PWM_MAX 50`, `CMDBUFFERSIZE`, `DEFAULT_FEEDRATE`, and the axis-inversion defines.
- **The slicer depends on this firmware's contract.** The authoritative write-up of that contract lives in the slicer repo, not here: `../software/AGENTS.md` and `../software/docs/OPAL_INTEGRATION.md`. The essentials it asserts: coordinates are mm in `0..field_size`, with `(0,0)` at a **corner**. Feedrate `F` is mm/s, not mm/min. The laser is gated by `M3`/`M5` rather than an inline `S`. Thus `G0` jumps must be wrapped in `M5`. A change to any of those here breaks generated G-code silently.
- `X_MAX`/`Y_MAX` here (250 mm) must stay equal to `field_size_x`/`field_size_y` in `../software/config/slicer.yaml`. They agree as of 2026-09-22.

## Build

```bash
pio run -e teensy40      # build for teensy40
pio run -t upload        # flash a connected Teensy
sh test/native/run.sh    # G-code parser tests on the PC (g++, address sanitizer), 5 tests
```

CI builds the firmware and runs the parser tests. The parser (`src/SerialCMDReader.*`) is the only part that runs without the board: the tests feed bytes through a fake `Serial` (`test/native/stubs/Arduino.h`). Motion, the galvo protocol and the laser cannot be verified without hardware. That is the strongest argument for changing them as little as possible.

## Security rules

These rules apply to every change in this repository. CI enforces most of them. Thus a red Security run means a rule broke. It does not mean CI is flaky. Each rule states its reason. Do not drop a rule because the reason looks unlikely: most of them come from a real incident.

### CI workflows (`.github/workflows/`)

- Pin every action to a full commit SHA. Put the version in a comment: `uses: actions/checkout@3d3c42e5aac5ba805825da76410c181273ba90b1 # v7.0.1`. A tag can be moved to other code. In March 2026, 76 of the 77 tags of `aquasecurity/trivy-action` were moved to malicious commits.
- Start every workflow with `permissions: {}`. Give each job only the permissions it uses. Usually this is `contents: read`.
- Set `persist-credentials: false` on every checkout. Set `timeout-minutes` on every job.
- Never put `${{ }}` with event data (a branch name, a tag, a PR title) inside `run:`. Pass it through `env:` and quote it: `"$TAG"`. Git allows `;` and `$(` in ref names. Thus a direct interpolation can run shell code.
- Run a scanner as a digest-pinned image (`name@sha256:...`), not as a third-party action. The trivy compromise came through the action layer.
- The Security workflow lints the workflows with actionlint and zizmor. Run both before you push a workflow change: `docker run --rm -v "$PWD:/repo" -w /repo rhysd/actionlint:1.7.12` and `pip install zizmor==1.30.1 && zizmor --offline --min-severity=low .github/workflows`. On Git Bash, prefix the docker command with `MSYS_NO_PATHCONV=1`.
- Do not rename a job. Branch protection finds a required check by its name. Thus a renamed job stops the check without an error.

### Dependencies

- Pin CI tools with `==` (PlatformIO, zizmor).

### Secrets

- Never commit a secret: no key, no password, no API token, no certificate key.
- gitleaks scans the whole git history. Deleting a file does not remove a secret from history. Thus a committed secret must be rotated.
- An entry in `.gitleaksignore` means: this credential was rotated. Never add an entry for a live credential.

### This repository

- The repository is public. Treat every commit as published.
- File names are case-sensitive on Linux. Match each `#include` to the file name exactly. CI builds on Linux.

### Proving a fix

- Show a security check fail on the bad input before you trust it to pass on the fix. A check that never failed may check nothing.

## Decisions (dated, append-mostly)

- 2026-09-22 - **A `.gitattributes` was added and the working tree renormalized to LF.** The repo had none. Thus a Windows clone checked out 42 text files as CRLF while the index held LF. That makes diffs between machines show whole files as changed. There is no functional effect on firmware that is compiled, but it hides real changes in noise.
- 2026-09-23 - **CI builds the firmware on Linux.** Its first run found a real break: `src/SerialCMDReader.*` included `helpers.h`, and the file is `Helpers.h`. Windows and macOS ignore case in file names, Linux does not. Fixed. The build also warns that `LASER_RESOLUTION` is defined three times. All three are `12`, and the one in `Helpers.h` ends with a stray `;`. Left as is, because the values agree.
- 2026-09-23 - **No `SECURITY.md` and no Dependabot.** Decided by Solvita. A public reporting address invites mail that nobody triages. Automated update pull requests are not wanted. Thus nothing updates a pin by itself. The weekly Security run finds a new CVE, and a person bumps the pin.
- 2026-09-24 - **`configuration.h` is now the one source for the field size, the feed rate, the laser type and the laser resolution.** `Helpers.h` repeated all four as its own literals. The coordinate mapper reads `X_MAX_POS_MM`, a `Helpers.h` literal. Thus editing `X_MAX` in `configuration.h`, as the docs say, changed nothing: a build with `X_MAX 200` gave a byte-identical firmware. Now `Helpers.h` includes `configuration.h` and defines `X_MAX_POS_MM` as `X_MAX`. Proof: the normal build is byte-identical before and after, and `X_MAX 200` now changes the firmware. The stray `;` on `LASER_RESOLUTION` and both redefinition warnings are gone. `lib/LaserController/Synrad48Ctrl.cpp` keeps its own `LASER_RESOLUTION 12`, because the library does not see `src/`.
- 2026-09-29 - **The G-code parser has host tests, and three read-past-buffer defects are fixed** (`aecdde0`). (1) `search_string` copied up to 10 characters into `char temp[10]`; a 10-character number had no terminator, and `strtod` read past the buffer. The buffer is now `NUMBER_CHARS_MAX + 1`. (2) A comment line (`;`, `(`, `/`) returned before the buffer and `cnt` were cleared. Thus the next line was scanned with the comment's characters: a `Y99` in a comment became the Y of a following `G1 X10`, and a few comment lines in a row pushed `cnt` past the 150-byte buffer and, past 255, made the `byte` loop index in `has_command` wrap forever (a hang). (3) `cnt` counted arriving bytes, not stored ones, so a cut line (over 149 characters) scanned past the buffer. The slicer and the host send none of these today: its numbers are at most 7 characters, and `galvo/gcode_layers.py` drops comment lines, with the note that OPAL "does not reliably ok comment lines" and "wedges the board": that wedge is defect (2). Thus the fixes change nothing for today's G-code; they close the cases that a hand-written file or another host would hit. Why these were safe to change without the board: for every line under 150 characters without a comment before it, the parser stores and scans the same bytes as before. `test/native/run.sh` fails on the old code in 4 of 5 tests (a stack overflow, a global overflow twice, a wrong Y). The test replaces `strtod` with one that calls `strlen` first, because the C library's `strtod` is not instrumented and hid the stack read.

- 2026-10-02 - **The laser fires only while the mirror scans, and the pin switches at once** (`feature/diode-laser-timing`). Found with the simulator (`test/sim/`): the host sends one line per `ok` and OPAL answers `ok` when it reads a line, so the queue was empty between lines and the beam waited, lit, through every round trip; the 1 kHz PWM took up to 1 ms to go dark, so a fuller queue lit every jump. Now `M3` arms the beam, a G1 lights it, its end darkens it (unless the next queued command is a G1); `G0`/`G28` disarm it and wait for the mirror; off is GPIO low and on is an immediate duty load with a new PWM period (`lib/LaserController/FlexPwmTiming.*`, FlexPWM2 submodule 2 = pin 6). Measured in `docs/benchmarks/`. A beam that stays lit on a standing mirror (an alignment spot) no longer exists: scan a short G1 back and forth instead.

## Gotchas (environment and process)

- **The `INVERSE_X` / `AXIS_INVERSE_X` macro mismatch is real and live.** Verified 2026-09-22: `src/configuration.h:39` defines `INVERSE_X`, while `src/main.h:46` tests `#ifdef AXIS_INVERSE_X` and `src/main.cpp:89` branches on the resulting `AXIS_INVERSE_X` bool. The names differ. Thus defining `INVERSE_X` has no effect, and axis inversion stays off. The slicer repo records this as known. It says the team deliberately left it that way, so the slicer compensates on its side. **If you ever repair this, the slicer's `invert_y` must change in the same commit.** Otherwise the two corrections cancel and parts come out mirrored.
- **Exactly one layer may mirror an axis.** The machine mirrors Y and the slicer cancels it (`invert_y` in `config/slicer.yaml`, default true, measured 2026-08-13). Firmware and slicer both doing it, or neither, are both wrong and both look plausible until a part comes off the plate.

## Current state (overwrite on change)

As of 2026-10-02:

- Branch **`master`** is the upstream code plus nothing; Optonics work lives on branches. **`feature/diode-laser-control`** is Lenards' diode firmware, the one on the OSLS-1, with `parser-fixes` merged (`07ac23e`) and the simulator (`7d13016`). **`feature/diode-laser-timing`** is that plus the timing fixes (`d43cb3d`): not on the board until benched.
- `pio run -e teensy40` SUCCESS on both branches. `sh test/native/run.sh` passes; the galvo and laser map tests pass; `sh test/sim/run.sh`: 39 tests pass, 97 % of lines (every reachable line), gate 97 % in CI.

## Open threads (overwrite on change)

- **Bench `feature/diode-laser-timing` before flashing it for real (Lenards, needs the board and a scope).** (1) Laser pin 6 and the galvo sync on a scope: the beam goes dark within microseconds of a G1 end, lights within microseconds of a G1 start, and never during a G0. (2) Measure the mirror lag with a 10 mm G0 and the position feedback; set `JUMP_DELAY_MIN_US` to about three times the lag and `LASER_ON_DELAY_US`/`LASER_OFF_DELAY_US` to about the lag (`docs/benchmarks/laser-timing-after.md`, point 5). (3) Measure one pass of `loop()` (toggle a free pin) and put it in `test/sim/sim.h`.
- **Then switch the host to a window.** `osls1_klipper/galvo/opal_link.py` has `OpalLink(window=FAST_OFF_WINDOW)`; the default stays 1 until this firmware is on the board. With the old firmware a window lights every jump.
- **1 kHz PWM leaves 20 % of every longer scan line dark at S204.** `DIODE_PWM_FREQ_HZ` in `DiodeLaserCtrl.h`. Raise it only if the diode driver's datasheet allows a faster modulation input.
- **`INVERSE_X` does nothing** (Gotchas). The compiler warns that `AXIS_INVERSE_X` is unused. Decide with the slicer's `invert_y` before changing it.
- The skills and the board fork notes of 2026-09-29 are superseded: the board firmware is `feature/diode-laser-control`, now on GitHub.
