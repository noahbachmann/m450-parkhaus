# Architecture

What's specified by the PDFs is stated as fact. Items marked **(open)** are undecided. Record the decision
here when it's made.

## Hardware

- 2 × **Raspberry Pi Pico W** (WiFi needed) → PlatformIO `board = rpipicow`
  (current `platformio.ini` still says `pico`; change only after asking).
- Pico 2 drives 2 × SG90 servos (entry, exit) and reads 2 safety loops (push button or IR sensor).
  Servo power from VBUS 5 V. Example signal pin from the assignment: **GP8**.
- Pin map beyond that: **(open)**.
- Separate test WLAN, **fixed IPs** for both Picos. Credentials handling **(open)**; don't commit secrets.

## Responsibilities

| Pico 1 "Kasse" (env `kasse`) | Pico 2 "Schranken" (env `schranken`) |
|---|---|
| Kasse web UI: search plate, show entry time/duration/fee, record payment | Registration web UI: add/list/delete plates (name + plate), persistent |
| Automat web UI: pay by plate, card/TWINT/cash + change | Camera simulation UI: entry and exit plate input, optional entry time, (later) call button, abort |
| Tariff calculation ([tariff.md](tariff.md)) | Barrier control: 2 servos, open/close |
| Parking sessions, **persisted in flash** (survive reboot) | Safety loops + safety logic (later) |
| Payment-status endpoint for Pico 2 | Event log (later): remote openings, safety stops, faults |
| Endpoint to open a session at entry (and close at exit) | Operator view, user «betrieb» (later) |

The registration list lives on Pico 2 (Testkonzept test object 01). Sessions live on Pico 1.

## Flows

**Entry** (Pico 2): normalize plate → validate → registered? (no → stay closed, «… nicht registriert,
bitte Ruftaste drücken») → ask Pico 1 to open a session with timestamp (optional backdated entry time)
→ Pico 1 rejects if a session is already open («Parkvorgang bereits offen – bitte Ruftaste drücken»)
→ success → servo opens (≤ 2 s total) → closes after the loop is released.
Pico 1 unreachable → stay closed, show fault, **no buffered session**.

**Payment** (Pico 1): look up open session by plate → compute fee → simulated method → mark paid with
method, amount and transaction reference (≤ 3 s). Cash accumulates partial amounts until the fee is covered, then
returns change. Already paid → «bereits bezahlt». No open session → «Kein offener Parkvorgang für <PLATE>».

**Exit** (Pico 2): normalize → query Pico 1 → `bezahlt`, or fee CHF 0.00 within free time → open
servo, Pico 1 closes the session with the exit time (clock stops). `unbezahlt` → closed,
«Gebühr CHF x.xx nicht bezahlt – bitte am Automaten bezahlen». `unbekannt` → closed. Timeout/error →
closed, «Kasse nicht erreichbar – bitte Ruftaste drücken». **Fail-safe: anything unexpected = closed.**

## Data

- **Plate:** normalize = uppercase and remove all whitespace. Valid = `^[A-Z]{2}[0-9]{1,6}$` after
  normalization. Shared pure function in `lib/` (Pico 2 validates input; Pico 1 validates API params).
- **Registration:** name, normalized plate (unique).
- **Parking session:** plate, entry time, exit time, amount, payment method, transaction ref, status.
  Status: `open` → `paid` → `closed`. Also `closed_free` («abgeschlossen (Freizeit)») and later
  `closed_unpaid` («ohne Zahlung ausgefahren»). At most one open/paid (not closed) session per plate.
  Unpaid exit attempt or abort leaves it `open`.
- **Time:** sessions need wall-clock time to the second, shown as `TT.MM.JJJJ HH:MM:SS`. Source
  **(open)**: NTP over WLAN vs. setting it from the browser. Logic in `lib/` receives the time as a parameter.
- **Storage:** flash filesystem (likely LittleFS) **(open)**. A test-data reset function is needed.

## Interface Pico 2 → Pico 1 (OP-01, format open)

Specified behaviour only:
- Status query by plate → `bezahlt` / `unbezahlt` / `unbekannt` (no open session), < 1 s.
- Empty/invalid plate → error response (e.g. HTTP 400), never a crash.
- Example from the Testkonzept: `GET /api/status?kz=LU123456`.
- Also needed: open session at entry (with optional entry time), close session at exit.
Define the exact endpoints and response bodies here before implementing.

## Code layout (decided)

One PlatformIO project:
- `[env:kasse]` and `[env:schranken]`: `board = rpipicow`, earlephilhower core, each with a
  `build_src_filter` selecting its own `src/` subfolder (e.g. `src/kasse/`, `src/schranken/`).
- `[env:native]` for host tests of `lib/` (tariff, plate, session state machine, change calculation).
- `lib/` stays free of Arduino headers. Hardware, WiFi, web server and storage glue live in `src/`.
- Hardware test suites run only on the board envs (`test_filter` / `test_ignore`).
