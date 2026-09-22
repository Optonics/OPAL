# AGENTS.md - OPAL galvo firmware

Persistent memory for this repo. Read the workspace `../AGENTS.md` first - it holds the cross-repo rules - then this file.

## The protocol

1. **Read this whole file before doing anything else.** It is short on purpose; if reading it feels expensive, prune it.
2. **Before ending a session, update it.** Ask: what did I learn that the next person (who may be me, months from now, with no memory of today) would otherwise have to rediscover? Write that. Skip anything derivable from the code, git history, or README - those already persist.
3. **Every entry is dated** (`YYYY-MM-DD`) and states *why*, not just *what*. Undated or unexplained entries are the first candidates for pruning.
4. **Convert relative time to absolute.** Never write "last week" or "soon"; write the date or the version number.
5. **Overwrite, don't accumulate.** "Current state" and "Open threads" describe *now* - rewrite them each time they change. "Decisions" and "Gotchas" are append-mostly - but delete entries that stopped being true, and say so in the commit message.
6. **Commit this file with the work it describes**, so the memory and the code never drift apart.
7. Formatting: plain hyphens only (no em or en dashes), one line per paragraph (no hard wrapping), named constants over repeated magic strings in any code you write.

## Project facts (stable)

- OPAL is firmware that turns G-code into the XY2-100 protocol that drives digital galvos, plus peripheral control. Upstream is the OpenGalvo project; this repo is Optonics's copy of it. Target is a Teensy 4.0 (not a 3.6) with an SN75174N on pins 22, 19, 17, 14. PlatformIO, `env:teensy40`, Arduino framework.
- Upstream calls it **pre-alpha and not ready for mainstream use**, with explicit warnings that the board, the galvo, the laser or something nearby can be destroyed. Treat any change as safety-relevant.
- `src/configuration.h` is the whole tunable surface and it is short: `X_MAX`/`Y_MAX` (both 250 mm), `LASER_IS_SYNRAD`, `LASER_RESOLUTION 12`, `LASER_PWM_MAX 50`, `CMDBUFFERSIZE`, `DEFAULT_FEEDRATE`, and the axis-inversion defines.
- **The slicer depends on this firmware's contract**, and the authoritative write-up of that contract lives in the slicer repo, not here: `../software/AGENTS.md` and `../software/docs/OPAL_INTEGRATION.md`. The essentials it asserts: coordinates are mm in `0..field_size` with `(0,0)` at a **corner**; feedrate `F` is mm/s, not mm/min; the laser is gated by `M3`/`M5` rather than an inline `S`, so `G0` jumps must be wrapped in `M5`. A change to any of those here breaks generated G-code silently.
- `X_MAX`/`Y_MAX` here (250 mm) must stay equal to `field_size_x`/`field_size_y` in `../software/config/slicer.yaml`. They agree as of 2026-09-22.

## Build

```bash
pio run                  # build for teensy40
pio run -t upload        # flash a connected Teensy
```

There is no test suite and no CI. Nothing in this repo can be verified without hardware, which is the strongest argument for changing it as little as possible.

## Decisions (dated, append-mostly)

- 2026-09-22 - **A `.gitattributes` was added and the working tree renormalized to LF.** The repo had none, so a Windows clone checked out 42 text files as CRLF while the index held LF, which makes diffs between machines show whole files as changed. No functional effect on firmware that is compiled, but it hides real changes in noise.

## Gotchas (environment and process)

- **The `INVERSE_X` / `AXIS_INVERSE_X` macro mismatch is real and live.** Verified 2026-09-22: `src/configuration.h:39` defines `INVERSE_X`, while `src/main.h:46` tests `#ifdef AXIS_INVERSE_X` and `src/main.cpp:89` branches on the resulting `AXIS_INVERSE_X` bool. The names differ, so defining `INVERSE_X` has no effect and axis inversion stays off. The slicer repo records this as known and says the team deliberately left it that way - so the slicer compensates on its side. **If you ever repair this, the slicer's `invert_y` must change in the same commit**, or the two corrections cancel and parts come out mirrored.
- **Exactly one layer may mirror an axis.** The machine mirrors Y and the slicer cancels it (`invert_y` in `config/slicer.yaml`, default true, measured 2026-08-13). Firmware and slicer both doing it, or neither, are both wrong and both look plausible until a part comes off the plate.

## Current state (overwrite on change)

As of 2026-09-22:

- Branch is **`master`**, not `main` - the only repo in the workspace that differs. Even with `origin`.
- **This checkout is an unmodified upstream snapshot.** All 57 commits are by the upstream author (`Bx3mE` / `RogueNeurons`, `c.daniel.olsson@gmail.com`) and the most recent is `2024-11-02 Update README.md`. There are no Optonics commits, and every remote branch is an upstream one.

## Open threads (overwrite on change)

- **The firmware the slicer describes is not the firmware in this repo, and it is worth finding out which one is on the board.** `../software/AGENTS.md` states that the ~1 mm integer `map()` resolution limit is "**fixed** in our fork (float `GalvoMap.h`, ~3.8 um/count)" and that `field_size` must match `X_MAX_POS_MM`/`Y_MAX_POS_MM`. Verified 2026-09-22: this repo contains **no `GalvoMap.h`**, no `X_MAX_POS_MM` and no `Y_MAX_POS_MM` (the defines here are `X_MAX`/`Y_MAX`), and no Optonics changes at all. So either the real fork lives somewhere outside this workspace and this repo is a stale upstream mirror, or the slicer is generating G-code for a firmware that was never flashed. The slicer assumes a ~3.8 um addressable step; the upstream integer `map()` cannot resolve below roughly 1 mm, and that difference does not announce itself - it comes out as geometry that is subtly wrong. Resolve this before trusting any dimensional result from the machine, and record the answer here.
