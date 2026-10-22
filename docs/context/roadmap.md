# Roadmap

Implementation steps for phase 1 (see [index.md](index.md)) and the later Testkonzept scope.
Each step ends with something testable. Update the status column when a step is done.

**Verification is done by the user** on their machine (`pio test`, `pio run`, flashing). The agent
implements a step, sets it to "awaiting user verification" and lists what to check. Only the user's
confirmation moves a step to "done". Failures they report are fixed in the same step.

| # | Step | Status |
|---|---|---|
| 1 | Foundation: envs, plate + tariff libs, native tests | done (verified by user 2026-10-19) |
| 2 | Domain logic in `lib/` | awaiting user verification |
| 3 | Shared board glue | open |
| 4 | Pico 1 "Kasse" firmware | open |
| 5 | Pico 2 "Schranken" firmware | open |
| 6 | Integration + phase-1 system test | open |
| 7 | Phase 2: Testkonzept scope | later |

## 1 — Foundation

- `platformio.ini`: `env:kasse` + `env:schranken` (`board = rpipicow`, `build_src_filter` per
  `src/<env>/`), `env:native`.
- `include/pins.h`, `include/secrets.example.h`.
- `lib/plate`: `normalizePlate`, `isValidPlate`. `lib/tariff`: `calculateFee` (integer Rappen).
- Unity suites `test/native/test_plate/plate.test.cpp`, `test/native/test_tariff/tariff.test.cpp` (R-01..R-12, KZ table, TF-19/25/27 values).
- **Done when:** `pio test -e native` green, `pio run -e kasse` and `-e schranken` build.
- **User verifies:** `pio test -e native` → 2 suites, 9 cases passed; `pio run -e kasse` and
  `pio run -e schranken` → `SUCCESS`; `include/pins.h` matches the wiring; constants in
  `lib/tariff/tariff.h` match [tariff.md](tariff.md).
- Covers: test goal 04 (dev level), plate rules for TF-02/03/06, Testkonzept start criterion
  "dev tests green".

## 2 — Domain logic in `lib/` (native-tested)

- Registration list: add (name + plate, normalized, unique), list, delete.
- Session store + state machine: `open → paid → closed / closed_free`; at most one non-closed
  session per plate; backdated entry; `entry_in_future`; exit rules (paid or fee 0 → close).
- Payment: Karte OK, Karte abgelehnt, TWINT, Bargeld (partial amounts accumulate, change);
  transaction reference from an injected counter; «bereits bezahlt» on double payment.
- Formatting: `CHF 0.00`, `TT.MM.JJJJ HH:MM:SS` in Europe/Zurich (CET/CEST rule as a pure function),
  duration `1 h 20 min`.
- JSON (de)serialization of registrations and sessions (ArduinoJson builds natively too).
- Pico 1 API handlers as pure functions: request → HTTP status + JSON body, per the interface in
  [architecture.md](architecture.md).
- **Done when:** all of the above covered by native Unity suites.
- **User verifies:** `pio test -e native` → 8 suites (plate, tariff, format, registration, session,
  payment, codec, kasse_api), 61 cases passed; `pio run -e kasse` and `pio run -e schranken` still
  `SUCCESS`; the "Domain rules" section in [architecture.md](architecture.md) matches what you want.

## 3 — Shared board glue (`src/common/` or a board-only lib)

- WiFi with fixed IP from `secrets.h`, NTP sync, LED (solid = ready, blinking = WLAN/NTP not ready).
- LittleFS atomic write (`<name>.tmp` → rename to `<name>.json`), load at boot.
- Test-data reset defaults matching the KZ table.
- **Done when:** both Picos join the WLAN, sync time, and persist/reload a test file across reboot.

## 4 — Pico 1 "Kasse" firmware

- WebServer port 80: `GET /api/status`, `POST /api/entry`, `POST /api/exit`.
- Kasse page (search plate, entry time, duration, fee, record payment) and Automat page.
- Sessions persisted in `/sessions.json`.
- **Done when:** TF-29 via curl passes; TF-18..22 pass with entries created via curl.

## 5 — Pico 2 "Schranken" firmware

- Registration page (persisted in `/registrations.json`).
- Camera simulation page: entry/exit plate, optional backdated entry time.
- HTTP client to Pico 1 (1000 ms timeout; any error → barrier stays closed).
- Servos: open on success (≤ 2 s total), close after `BARRIER_MIN_OPEN_MS` (5 s) once the safety loop
  is free. Loops are optional hardware: must work with nothing connected to GP14/GP15.
- **Done when:** TF-01..04, 06, 07, 10, 11, 23..25, 30 pass against a running Pico 1.

## 6 — Integration + phase-1 system test

- Full TU-2 setup; run all phase-1 TFs incl. TF-08 timing and TF-32; fix defects.
- Update the German `README.md` (implemented user stories table).

## 7 — Phase 2 (Testkonzept scope, later)

- Safety loop logic: no closing while occupied, stop + reopen during closing, 5-min fault message.
- Event log on Pico 2; operator view (user «betrieb»), remote opening with mandatory reason.
- Call button, exit abort («Fahrzeug wendet»), payment at the exit terminal incl. 5 declined attempts.
- Tariff page in Kasse and Automat; terminal feedback texts (TF-05, TF-26).
- Hardware test suites under `test/board/` + per-env `test_filter` (`native/*` vs `board/*`); tag `v1.0`.
