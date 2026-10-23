# Roadmap

Implementation steps for phase 1 (see [index.md](index.md)) and the later Testkonzept scope.
Each step ends with something testable. Update the status column when a step is done.

**Verification is done by the user** on their machine (`pio test`, `pio run`, flashing). The agent
implements a step, sets it to "awaiting user verification" and lists what to check. Only the user's
confirmation moves a step to "done". Failures they report are fixed in the same step.

**No hardware from step 4 on (decided 2026-10-23):** the user can no longer run the Picos. Every step
is verified with `pio test -e native` + `pio run` only. Logic goes into `lib/` so it is native-tested;
`src/` is thin glue that must compile. The phase-1 TFs run as a native in-process system test (step 6);
what only hardware can show (servo motion, ≤ 2 s timing, WLAN, real power cycle) is documented as not
verified. No `test/board/` suites and no `v1.0` tag.

| # | Step | Status |
|---|---|---|
| 1 | Foundation: envs, plate + tariff libs, native tests | done (verified by user 2026-10-19) |
| 2 | Domain logic in `lib/` | done (verified by user 2026-10-19) |
| 3 | Shared board glue | done (verified by user 2026-10-22) |
| 4 | Pico 1 "Kasse" firmware | done (verified by user 2026-10-23, native + build) |
| 5 | Pico 2 "Schranken" firmware | done (verified by user 2026-10-23, native + build) |
| 6 | Integration + phase-1 system test | done (verified by user 2026-10-23, native + build) |
| 7a | Phase 2: barrier safety | done (verified by user 2026-10-23, native + build) |
| S | PC simulator (Docker / `sim_kasse` + `sim_schranken`) | done (verified by user 2026-10-23, Docker) |
| 7b | Phase 2: event log + operator view | done (verified by user 2026-10-23, native + build) |
| 7c | Phase 2: exit terminal | done (verified by user 2026-10-23, native + build + Docker) |
| 7d | Phase 2: tariff page + wrap-up | awaiting user verification |

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

## 3 — Shared board glue (`src/common/`, built into both envs via `build_src_filter`)

- WiFi with fixed IP from `secrets.h` (non-blocking `beginNoBlock`, retry every 20 s), NTP sync,
  LED (solid = ready, blinking = WLAN/NTP not ready). LED state is tracked in a variable: on the
  Pico W `digitalRead(LED_BUILTIN)` does not return the written value.
- LittleFS (`board_build.filesystem_size = 0.5m`) atomic write (`<name>.tmp` → rename to
  `<name>.json`), load at boot, defaults written if the file is missing or invalid.
- Test-data reset defaults matching the KZ table: `lib/testdata` (native-tested).
- Temporary smoke-test `main.cpp` on both boards: boot counter in `/boot.json`, status line every 5 s.
- **Done when:** both Picos join the WLAN, sync time, and persist/reload a test file across reboot.
- **User verifies:** `pio test -e native` → 9 suites, 63 cases passed; `pio run` → both `SUCCESS`;
  per Pico: LED blinks then goes solid, serial shows IP, Zurich time (±2 s), boot counter increases
  after each power cycle; hotspot off → LED blinks, back on → reconnects within ~20 s.

## 4 — Pico 1 "Kasse" firmware

- WebServer port 80: `GET /api/status`, `POST /api/entry`, `POST /api/exit`.
- Kasse page (search plate, entry time, duration, fee, record payment) and Automat page.
- Sessions persisted in `/sessions.json`.
- Page texts, cash input parsing, pay-form handling (`handlePayForm`) and page HTML (`renderPage` in
  `lib/kasse_view/kasse_pages.cpp`) are pure functions in `lib/kasse_view` (native-tested); routes in
  `src/kasse/main.cpp`. Test-data reset `POST /reset`. Step-3 boot counter removed from Kasse.
- **Done when:** native tests cover TF-19..22 on the rendered pages and the pay form; TF-18/TF-29 are
  covered by the `kasse_api` suite and the step-6 system test.
- **User verifies:** `pio test -e native` → 10 suites, 81 cases passed; `pio run` → both `SUCCESS`.
  (The on-board walkthrough `docs/anleitung-schritt-4-kasse.md` can't be run any more.)

## 5 — Pico 2 "Schranken" firmware

- Registration page (persisted in `/registrations.json`).
- Camera simulation page: entry/exit plate, optional backdated entry time.
- HTTP client to Pico 1 (1000 ms timeout; any error → barrier stays closed).
- Servos: open on success (≤ 2 s total), close after `BARRIER_MIN_OPEN_MS` (5 s) once the safety loop
  is free. Loops are optional hardware: must work with nothing connected to GP14/GP15.
- Logic in `lib/barrier`, `lib/gate` (via `KasseLink`), `lib/schranken_view`, `parseLocalDateTime` in
  `lib/format`, shared HTML in `lib/html`; glue in `src/schranken/main.cpp`. Step-3 boot counter removed.
- **Done when:** native suites cover registration (TF-01..03), entry/exit decisions incl. fail-safe on
  timeout and malformed replies (TF-04, 07, 10, 11, 23, 24, 30), barrier timing and backdated entry time.
- **User verifies:** `pio test -e native` → 13 suites, 106 cases passed; `pio run` → both `SUCCESS`.

## 6 — Integration + phase-1 system test

- Native system test `test/native/test_system/`: Pico 2 logic (registrations, `lib/gate`, `lib/barrier`)
  calls Pico 1 logic (`kasse_api` handlers, `handlePayForm`, pages) through an in-process `KasseLink`
  with a simulated clock and a "Kasse off" switch; reboot = codec round-trip. One test per phase-1 TF:
  01–04, 06–11, 18–25, 29, 30, 32. No defects found.
- German `docs/testprotokoll.md` (results + what only hardware could show), `README.md` user story table.
- **User verifies:** `pio test -e native` → 14 suites, 127 cases passed; `pio run` → both `SUCCESS`.

## 7 — Phase 2 (Testkonzept scope), split into 7a–7d with a user check after each

Hardware test suites under `test/board/` and the `v1.0` tag are dropped (no hardware, decided 2026-10-23).

### 7a — Barrier safety

- `lib/barrier`: states closed / open / closing. Closing is a 2 s sweep (`BARRIER_CLOSE_MS`); no closing
  while the loop is occupied; loop triggered while closing → stop and fully up in the same update
  (`SafetyStop` event), minimum open time restarts; loop occupied ≥ 5 min → `LoopFault` event once,
  message «Sicherungsschleife Einfahrt länger als 5 min belegt» on the camera page, barrier stays open.
- Firmware: barriers + loops run on core 1 (`setup1`/`loop1`) so a blocking HTTP call on core 0 can't
  delay a stop; cores share only single-writer atomics. Events go to Serial (event log in 7b).
- **User verifies:** `pio test -e native` → 14 suites, 133 cases passed; `pio run` → both `SUCCESS`.

### S — PC simulator (added 2026-10-23, user request)

- Two programs like the two boards: `src/sim/kasse/` (Pico 1, API + pages + sessions, port 8081) and
  `src/sim/schranken/` (Pico 2, port 8082) calling the Kasse over real HTTP (1000 ms timeout).
  Shared glue in `src/sim/common/` (own HTTP server/client, Winsock or POSIX).
- Docker: `docker compose up --build` (services `kasse`, `schranken`). Without Docker:
  `pio run -e sim_kasse -t exec` and `pio run -e sim_schranken -t exec` in two terminals.
- Found while testing (open, needs a decision): if Pico 1 answers an entry only after Pico 2's 1000 ms
  timeout, the session is opened although the barrier stayed closed; the retry then gets
  «Parkvorgang bereits offen». Affects the firmware too.
- **User verifies:** `docker compose up --build`, then in the browser: register, entry (also backdated),
  pay, exit, loop toggles, `docker compose stop kasse` / `start kasse`. Optional:
  `pio run -e sim_kasse` and `-e sim_schranken` build on Windows. `pio test -e native` and `pio run`
  unchanged (14 suites, 133 cases).

### 7b — Event log + operator view

- `lib/event_log` (+ `/events.json` via `lib/codec`), `lib/operator_view` (lane status, remote opening,
  logging helpers, `/betrieb` page). Pico 1: status `closed_unpaid`, `SessionStore::forceExit`,
  `/api/exit` `override`. `KasseLink` gained `getJson` for `/api/status`; `GateResult.unreachable`.
- Firmware + simulator: `/betrieb` (Basic auth «betrieb»), `/betrieb/oeffnen`; safety stops, loop faults,
  Kasse-unreachable and remote openings are logged; Pico 2 reset also clears the log.
  `OPERATOR_PASSWORD` added to `secrets.example.h`.
- Tests: TF-14, 15 (log), 16, 17, 30 (fault logged), 31 in the system test; unit suites `event_log`,
  `operator_view`; `forceExit`, `override`, `closed_unpaid` codec.
- First check by the user passed (2026-10-23). Then, at the user's request: remote opening stays closed
  unless Pico 1 confirms (Kasse down → logged fault), valid plate required.
- **User verifies:** `pio test -e native` → 16 suites, 156 cases passed; `pio run` → both `SUCCESS`;
  `docker compose up --build` → `http://localhost:8082/betrieb` (betrieb / parkhaus).

### 7c — Exit terminal

- `lib/terminal` (`LaneTerminal`: last plate, payment offer, decline counter, open call), Pico 1
  `POST /api/pay` (`handlePay`), event type «Zahlung 5× abgelehnt», `GateResult.unpaid/feeRappen`.
- Camera page: «Ruftaste» on both lanes, «Fahrzeug wendet» + payment form on the exit lane; operator lane
  shows the open call. Firmware + simulator routes `/kamera/ruftaste`, `/kamera/abbrechen`,
  `/kamera/bezahlen`, `/api/pay`.
- Tests: TF-05, 26, 27, 28 + call button in the system test; `test_terminal`; `/api/pay` in `kasse_api`.
- **User verifies:** `pio test -e native` → 17 suites, 171 cases passed; `pio run` → both `SUCCESS`;
  `docker compose up --build` → exit terminal on `/kamera`, call shown on `/betrieb`.

### 7d — Tariff page + wrap-up

- `GET /tarif` on Pico 1 (firmware + simulator): `tariffSummary()` (tariff.md wording) and example fees
  computed by `calculateFee`, linked from Kasse and Automat (US-14 AK3, rest of TF-19).
- README (user stories incl. phase 2, pages, known limitations), test protocol (TF-01..32, not run:
  TF-33/34, known defect), architecture.
- **User verifies:** `pio test -e native` → 17 suites, 174 cases passed; `pio run` → both `SUCCESS`;
  `docker compose up --build` → `http://localhost:8081/tarif`.

## Open after step 7

- Late entry reply (Pico 1 answers after Pico 2's 1000 ms timeout → session opened, barrier closed):
  needs a decision, see step S. Listed as a known defect (class 2) in `docs/testprotokoll.md`.
- TF-33 / TF-34 are manual tests; they can be run with the PC simulator.
- Firmware has never run on hardware (built only).
