# Architecture

Facts from the PDFs plus design decisions (settled 2026-10-15). Mark new undecided items **(open)** and
record the decision here when it's made.

## Hardware

- 2 × **Raspberry Pi Pico W** (WiFi needed) → PlatformIO `board = rpipicow`.
- Pico 2 drives 2 × SG90 servos (entry, exit) and reads 2 safety loops (push button or IR sensor).
- Onboard LED on **both** Picos shows status: solid = ready, blinking = WLAN or NTP not ready.
  (On the Pico W the LED is on the CYW43 chip: `LED_BUILTIN` works through the core.)

### Pin map Pico 2 (user may change it)

All pins are defined in **one place, `include/pins.h`**. If the user rewires, only that file and this
table change.

| Function | GPIO | Phys. pin | Mode |
|---|---|---|---|
| Entry servo signal | GP8 | 11 | PWM (Servo lib) |
| Exit servo signal | GP9 | 12 | PWM (Servo lib) |
| Entry safety loop | GP14 | 19 | `INPUT_PULLUP`, LOW = occupied |
| Exit safety loop | GP15 | 20 | `INPUT_PULLUP`, LOW = occupied |
| Servo power | VBUS 5 V | 40 | — |
| GND | GND | 8 / 13 | — |

Pico 1 has no external hardware.

### WLAN credentials and IPs

- `include/secrets.h` (**gitignored**) holds SSID, password and the fixed IPs of both Picos
  (`KASSE_IP`, `SCHRANKEN_IP`, gateway, subnet).
- `include/secrets.example.h` (committed) shows the format. Copy it to `secrets.h` and fill it in.
- Separate test WLAN (phone hotspot). It needs internet access for NTP.

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
- **Time:** **NTP over WLAN** at boot on both Picos. Internally Unix epoch seconds (UTC); displayed in
  Europe/Zurich (`TZ = "CET-1CEST,M3.5.0,M10.5.0/3"`) as `TT.MM.JJJJ HH:MM:SS`. **Pico 1 is
  authoritative for session timestamps** (entry and exit time are stamped by Pico 1 on receiving the
  request). Pico 2 sends a time only for a backdated entry. As long as NTP hasn't synced, entry/exit are
  rejected (barrier closed) and the LED blinks. Logic in `lib/` gets the time as a parameter.
- **Storage:** **LittleFS + JSON (ArduinoJson)**. All data is held in RAM (structs) and loaded at boot.
  On every change the file is written to `<name>.tmp`, then renamed over `<name>.json` (atomic, survives
  a power cut during a write; TF-11/TF-30 pull the power on purpose).
  Pico 1: `/sessions.json`. Pico 2: `/registrations.json` (later also `/events.json`).
  A **test-data reset** (button/endpoint on each Pico) rewrites the files from built-in defaults
  matching the KZ table in test-concept.md. No database: a Pico W has no room for one and the data is
  tiny (tens of records).

## Interface Pico 2 → Pico 1 (OP-01, decided)

JSON over HTTP on Pico 1, port 80. Requests with a body use `Content-Type: application/json`.
All times are Unix epoch seconds, amounts are integer **Rappen**. Plates may be sent unnormalized:
Pico 1 normalizes and validates them. Every error is `{"error":"<code>"}` with the given HTTP status.
Pico 2 client timeout: **1000 ms** (keeps the entry ≤ 2 s). Timeout or connection error = Pico 1 unreachable.

**`GET /api/status?kz=<plate>`** — payment status (TF-29, < 1 s)
- 200 `{"kz":"LU123456","status":"bezahlt"|"unbezahlt"|"unbekannt","fee":400,"entry":1760530000}`
  (`unbekannt` = no open session; then `fee`/`entry` are omitted)
- 400 `invalid_plate` (empty or bad format)

**`POST /api/entry`** — open a session
- body `{"kz":"LU123456"}` or `{"kz":"LU123456","entry":1760521000}` (backdated entry time)
- 201 `{"kz":"LU123456","entry":1760530000}`
- 409 `already_open` · 400 `invalid_plate` · 400 `entry_in_future` · 503 `time_not_synced`

**`POST /api/exit`** — try to close the session; Pico 2 opens the barrier only on 200
- body `{"kz":"LU123456"}`
- 200 `{"kz":"LU123456","status":"closed"|"closed_free","exit":1760533600,"fee":400}`
- 402 `{"error":"unpaid","fee":400}` → «Gebühr CHF 4.00 nicht bezahlt – bitte am Automaten bezahlen»
- 404 `no_session` · 400 `invalid_plate` · 503 `time_not_synced`

Exit attempt without success (or abort) changes nothing: the session stays open and the clock keeps
running. The registration check happens on Pico 2 before `/api/entry`. Pico 1 doesn't know registrations.

The Kasse/Automat pages are served by Pico 1 itself and call its internal logic directly (no API
needed). Later phases may add JSON endpoints for them if the pages use fetch().

## Code layout (decided)

One PlatformIO project:
- `[env:kasse]` and `[env:schranken]`: `board = rpipicow`, earlephilhower core, each with a
  `build_src_filter` selecting its own `src/` subfolder (e.g. `src/kasse/`, `src/schranken/`).
- `[env:native]` for host tests of `lib/` (tariff, plate, session state machine, change calculation).
- Libraries: ArduinoJson (`lib_deps`), core's WiFi, WebServer, HTTPClient, LittleFS, Servo.
- `lib/` stays free of Arduino headers. Hardware, WiFi, web server and storage glue live in `src/`.
- Hardware test suites run only on the board envs (`test_filter` / `test_ignore`).
