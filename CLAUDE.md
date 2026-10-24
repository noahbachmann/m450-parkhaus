# Pico project — agent context

Car park barrier + payment system (school module M450) on two Raspberry Pi Pico W,
Arduino framework (earlephilhower core) via PlatformIO.

## Context docs — read first

Specs are condensed in `docs/context/` (English). Start with `docs/context/index.md`;
open the other files only as needed. The source PDFs in the repo root are untracked —
don't re-read them unless the context is missing something.

- **State:** the Testkonzept scope is implemented; the firmware is only built (no hardware), everything is
  verified natively and in the PC simulator. Docs describe the current state, not the history (that is git).
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
    pio run -e hwtest -t upload          wiring check for Pico 2 (servos + loops)
    pio device monitor -b 115200         serial console
    pio test -e native                   unit + system tests on the PC, no board
    docker compose up --build            PC simulator: UI localhost:8082 (Pico 2), Kasse API localhost:8081
    pio run -e sim_kasse -t exec         same without Docker (plus -e sim_schranken in a 2nd terminal)

## Layout

    lib/<name>/      logic, one folder per library
    lib/schranken_app/   all Pico 2 routes (SchrankenApp); lib/kasse_api routeKasse = all Pico 1 routes
    lib/test_support/    header-only test fixtures and the system-test rig
    src/kasse/       Pico 1 adapter (env kasse)
    src/schranken/   Pico 2 adapter (env schranken); lane.* = barrier on core 1
    src/common/      shared board glue (WLAN, NTP, LittleFS, LED, WebServer <-> lib/web)
    src/sim/         PC simulator adapters (envs sim_kasse, sim_schranken, Docker)
    src/hwtest/      wiring check (env hwtest)
    include/         pins.h, secrets.example.h (secrets.h is gitignored)
    test/native/test_<name>/<name>.test.cpp   suite (Unity), one folder = one program with its own main()

## Guidelines

- Logic lives in `lib/` and does not include `Arduino.h` or call `millis()`,
  `Serial`, `digitalWrite()` — otherwise it won't build under `native`.
- Time is passed in as a parameter, not read inside the logic.
- Side effects only in `src/`.
- Routes exist only in `lib/` (`SchrankenApp::ROUTES`, `routeKasse`); the board and simulator mains are
  adapters. A new page or form = a route + handler there and a test in `test_schranken_app` / the system suites.
- Suite folders must start with `test_` (`test_filter = native/*` on `native`).

## Style

- Allman braces: opening brace on its own line
- comments only where non-obvious; no file-header comment blocks

## Board

Hardware is two **Pico W** → target `board = rpipicow` (LED on the CYW43 chip).
Envs: `kasse` (Pico 1), `schranken` (Pico 2) and `hwtest`, each with `build_src_filter` on its
`src/` subfolder, plus `native` and the simulator envs.
