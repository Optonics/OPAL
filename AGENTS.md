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
pio run                  # build for teensy40
pio run -t upload        # flash a connected Teensy
```

There is no test suite and no CI. Nothing in this repo can be verified without hardware. That is the strongest argument for changing it as little as possible.

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

## Gotchas (environment and process)

- **The `INVERSE_X` / `AXIS_INVERSE_X` macro mismatch is real and live.** Verified 2026-09-22: `src/configuration.h:39` defines `INVERSE_X`, while `src/main.h:46` tests `#ifdef AXIS_INVERSE_X` and `src/main.cpp:89` branches on the resulting `AXIS_INVERSE_X` bool. The names differ. Thus defining `INVERSE_X` has no effect, and axis inversion stays off. The slicer repo records this as known. It says the team deliberately left it that way, so the slicer compensates on its side. **If you ever repair this, the slicer's `invert_y` must change in the same commit.** Otherwise the two corrections cancel and parts come out mirrored.
- **Exactly one layer may mirror an axis.** The machine mirrors Y and the slicer cancels it (`invert_y` in `config/slicer.yaml`, default true, measured 2026-08-13). Firmware and slicer both doing it, or neither, are both wrong and both look plausible until a part comes off the plate.

## Current state (overwrite on change)

As of 2026-09-22:

- Branch is **`master`**, not `main`. This is the only repo in the workspace that differs. It is even with `origin`.
- **This checkout is an unmodified upstream snapshot.** All 57 commits are by the upstream author (`Bx3mE` / `RogueNeurons`, `c.daniel.olsson@gmail.com`). The most recent is `2024-11-02 Update README.md`. There are no Optonics commits. Every remote branch is an upstream one.

## Open threads (overwrite on change)

- **The firmware the slicer describes is not the firmware in this repo.** It is worth finding out which one is on the board. `../software/AGENTS.md` states that the ~1 mm integer `map()` resolution limit is "**fixed** in our fork (float `GalvoMap.h`, ~3.8 um/count)". It also states that `field_size` must match `X_MAX_POS_MM`/`Y_MAX_POS_MM`. Verified 2026-09-22: this repo contains **no `GalvoMap.h`**, no `X_MAX_POS_MM` and no `Y_MAX_POS_MM` (the defines here are `X_MAX`/`Y_MAX`), and no Optonics changes at all. Thus either the real fork lives somewhere outside this workspace and this repo is a stale upstream mirror, or the slicer is generating G-code for a firmware that was never flashed. The slicer assumes a ~3.8 um addressable step. The upstream integer `map()` cannot resolve below roughly 1 mm. That difference does not announce itself. It comes out as geometry that is subtly wrong. Resolve this before trusting any dimensional result from the machine, and record the answer here.
