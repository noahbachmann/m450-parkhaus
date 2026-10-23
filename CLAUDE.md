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

    pio run                              build both boards (kasse, schranken)
    pio run -e kasse -t upload           flash Pico 1 (same with -e schranken)
    pio device monitor -b 115200         serial console
    pio test -e native                   host-side unit tests, no board
    docker compose up --build            PC simulator: Kasse localhost:8081, Schranken localhost:8082
    pio run -e sim_kasse -t exec         same without Docker (plus -e sim_schranken in a 2nd terminal)
    pio test -e kasse                    on-board tests, board required (later)

## Layout

    lib/<name>/      logic, one folder per library
    src/kasse/       Pico 1 hardware glue (env kasse)
    src/schranken/   Pico 2 hardware glue (env schranken)
    src/sim/         PC simulator glue (envs sim_kasse, sim_schranken, Docker): keep routes in sync with the boards
    include/         shared headers
    test/native/test_<name>/<name>.test.cpp   host-only suite (Unity), one folder = one program with its own main()
    test/board/test_<name>/<name>.test.cpp    on-board suite (later)

## Guidelines

- Logic lives in `lib/` and does not include `Arduino.h` or call `millis()`,
  `Serial`, `digitalWrite()` — otherwise it won't build under `native`.
- Time is passed in as a parameter, not read inside the logic.
- Side effects only in `src/`.
- Suite folders must start with `test_`. Once board suites exist, set `test_filter = native/*`
  on `native` and `test_filter = board/*` on `kasse` / `schranken` in `platformio.ini`.

## Style

- Allman braces: opening brace on its own line
- comments only where non-obvious; no file-header comment blocks

## Board

Hardware is two **Pico W** → target `board = rpipicow` (LED on the CYW43 chip).
Envs: `kasse` (Pico 1) and `schranken` (Pico 2), each with `build_src_filter` on its
`src/` subfolder, plus `native`. Implementation steps and status: `docs/context/roadmap.md`.
