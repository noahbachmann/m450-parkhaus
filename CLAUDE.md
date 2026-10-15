# Pico project — agent context

Raspberry Pi Pico, Arduino framework (earlephilhower core) via PlatformIO.
<!-- TODO: describe the project, requirements and where they live -->

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
- Side effects only in `src/main.cpp`.
- Once both kinds of test suites exist, separate them per env in
  `platformio.ini` with `test_filter` / `test_ignore` (host-only suites for
  `native`, hardware suites for `pico`).

## Style

- Allman braces: opening brace on its own line
- comments only where non-obvious; no file-header comment blocks

## Board

`board = pico` — plain Pico, LED on GPIO25. A Pico W needs `board = rpipicow`
(its LED sits on the CYW43 chip). Ask before changing.
