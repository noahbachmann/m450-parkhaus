# Pico project — agent context

Car park barrier + payment system (school module M450) on two Raspberry Pi Pico W,
Arduino framework (earlephilhower core) via PlatformIO.

## Context docs — read first

Specs are condensed in `docs/context/` (English). Start with `docs/context/index.md`;
open the other files only as needed. The source PDFs in the repo root are untracked —
don't re-read them unless the context is missing something.

- **Current phase 1:** assignment minimum + registration (registration, entry, session,
  fee, payment at Kasse/Automat, exit with status query). Testkonzept scope comes later.
- **Tariff:** `docs/context/tariff.md` is the source of truth. The user may edit it;
  then update the tariff code in `lib/` and its tests to match.
- Design decisions (API, time, storage, pins, secrets) are in `docs/context/architecture.md`.
  Pins live only in `include/pins.h`; the user may rewire, so keep that table and file in sync.

## Language

- Code, identifiers, comments, `docs/context/`: English.
- `README.md` and other docs for humans: German.
- UI strings: German, verbatim as in `docs/context/test-concept.md`.

## Commands

PlatformIO only. Parent `M450/CLAUDE.md` says cmake/make/ctest — wrong here.

    pio run -e pico                build
    pio run -e pico -t upload      flash
    pio device monitor -b 115200   serial console
    pio test -e native             host-side unit tests, no board
    pio test -e pico               on-board tests, board required

## Layout

    lib/<name>/      logic, one folder per library
    src/main.cpp     hardware glue: pins, Serial, timing
    include/         shared headers
    test/<suite>/    one folder per test suite (Unity)

## Guidelines

- Logic lives in `lib/` and does not include `Arduino.h` or call `millis()`,
  `Serial`, `digitalWrite()` — otherwise it won't build under `native`.
- Time is passed in as a parameter, not read inside the logic.
- Side effects only in `src/`.
- Once both kinds of test suites exist, separate them per env in
  `platformio.ini` with `test_filter` / `test_ignore` (host-only suites for
  `native`, hardware suites for `pico`).

## Style

- Allman braces: opening brace on its own line
- comments only where non-obvious; no file-header comment blocks

## Board

Hardware is two **Pico W** → target `board = rpipicow` (LED on the CYW43 chip).
Planned envs: `kasse` (Pico 1) and `schranken` (Pico 2) with `build_src_filter`, plus
`native`. `platformio.ini` still has the template `env:pico` with `board = pico` —
ask before changing it.
