# Architecture

Facts from the PDFs plus design decisions (settled 2026-10-15). Mark new undecided items **(open)** and
record the decision here when it's made.

## Hardware

- 2 × **Raspberry Pi Pico W** (WiFi needed) → PlatformIO `board = rpipicow`.
- Pico 2 drives 2 × SG90 servos (entry, exit) and reads 2 safety loops (push button or IR sensor).
  The loops are **optional hardware**: with nothing connected, `INPUT_PULLUP` reads HIGH = free, so the
  firmware runs unchanged without them (decided 2026-10-22).
- Onboard LED on **both** Picos shows status: solid = ready, blinking = WLAN or NTP not ready.
  (On the Pico W the LED is on the CYW43 chip: `LED_BUILTIN` works through the core.)

### Pin map Pico 2 (user may change it)

All pins are defined in **one place, `include/pins.h`**. If the user rewires, only that file and this
table change.

| Function | GPIO | Phys. pin | Mode |
|---|---|---|---|
| Entry servo signal | GP27 | 32 | PWM (Servo lib) |
| Exit servo signal | GP17 | 22 | PWM (Servo lib) |
| Entry safety loop | GP14 | 19 | `INPUT_PULLUP`, LOW = occupied |
| Exit safety loop | GP15 | 20 | `INPUT_PULLUP`, LOW = occupied |
| Servo power | VBUS 5 V | 40 | — |
| GND | GND | 33 / 23 | brown wire of entry / exit servo (next to its signal pin) |
| GND (buttons, optional) | GND | 18 | other leg of both buttons |

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
→ success → servo opens (≤ 2 s total) → closes once the minimum open time (`BARRIER_MIN_OPEN_MS` = 5 s)
has passed **and** the loop is free (without loop hardware: closes after 5 s). Same for the exit barrier.
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
A body that is not a JSON object with a string `kz` (or a non-integer `entry`) → 400 `bad_request`.
Pico 2 client timeout: **1000 ms** (keeps the entry ≤ 2 s). Timeout or connection error = Pico 1 unreachable.

**`GET /api/status?kz=<plate>`** — payment status (TF-29, < 1 s)
- 200 `{"kz":"LU123456","status":"bezahlt"|"unbezahlt"|"unbekannt","fee":400,"entry":1760530000}`
  (`unbekannt` = no open session; then `fee`/`entry` are omitted)
- 400 `invalid_plate` (empty or bad format)

**`POST /api/entry`** — open a session
- body `{"kz":"LU123456"}` or `{"kz":"LU123456","entry":1760521000}` (backdated entry time)
- 201 `{"kz":"LU123456","entry":1760530000}`
- 409 `already_open` · 400 `invalid_plate` · 400 `entry_in_future` · 503 `time_not_synced` ·
  503 `store_full` (50 sessions, none closed to drop)

**`POST /api/exit`** — try to close the session; Pico 2 opens the barrier only on 200
- body `{"kz":"LU123456"}`
- 200 `{"kz":"LU123456","status":"closed"|"closed_free","exit":1760533600,"fee":400}`
- 402 `{"error":"unpaid","fee":400}` → «Gebühr CHF 4.00 nicht bezahlt – bitte am Automaten bezahlen»
- 404 `no_session` · 400 `invalid_plate` · 503 `time_not_synced` (also when the clock is before the
  session's entry time)

Exit attempt without success (or abort) changes nothing: the session stays open and the clock keeps
running. The registration check happens on Pico 2 before `/api/entry`. Pico 1 doesn't know registrations.

The Kasse/Automat pages are served by Pico 1 itself and call its internal logic directly (no API
needed). Later phases may add JSON endpoints for them if the pages use fetch().

**Pico 1 web pages (step 4, decided 2026-10-22):** server-rendered HTML forms, no JavaScript.
- `GET /` → 303 to `/kasse`.
- `GET /kasse?kz=` — search: active session, else the latest closed one; list of all sessions; reset button.
- `GET /automat?kz=` — search: active session only; none → «Kein offener Parkvorgang für <PLATE>».
- `POST /kasse/pay`, `POST /automat/pay` — form fields `kz`, `method` (`card_ok|card_declined|twint|cash`),
  `amount` (CHF, cash only). Post/redirect/get: the result is kept in RAM and shown once after a 303 to
  `<page>?kz=<PLATE>&m=<id>`, so a reload doesn't pay twice. The pay form stays visible on a paid session so
  a second attempt shows «bereits bezahlt» (TF-20).
- `POST /reset` — test-data reset (no sessions, tx counter 1), 303 to `/kasse`.
- Sessions are written to flash after every successful entry, exit and payment (incl. partial cash).
- The JSON API reads the request body from WebServer's `plain` argument, which is only set for a
  non-form content type: clients must send `Content-Type: application/json`.

**Pico 2 web pages (step 5, decided 2026-10-23):** server-rendered HTML forms, same style as Pico 1.
- `GET /` → 303 to `/kamera`.
- `GET /kamera` — one card per lane: barrier state, last terminal text, plate field, «Erfassen»; the entry
  card also has an optional `datetime-local` entry time (Europe/Zurich, empty = now).
- `POST /kamera/einfahrt` (fields `kz`, `entry`), `POST /kamera/ausfahrt` (`kz`) → decision, 303 to `/kamera`.
- `GET /registrierung`, `POST /registrierung/add` (`name`, `kz`), `POST /registrierung/delete` (`kz`),
  `POST /reset` (default registrations); post/redirect/get with a one-time notice like Pico 1.
- Entry/exit decisions are pure functions in `lib/gate` behind a `KasseLink` interface (firmware:
  `HTTPClient`, 1000 ms timeout; tests: fakes or the real `kasse_api` handlers). Only a well-formed
  201 (entry) or 200 with `closed`/`closed_free` (exit) opens; everything else keeps the barrier closed.
- `lib/barrier` holds the barrier state machine (servo angles 0° closed, 90° open); the firmware writes
  the angle to the servo whenever it changes.

## Domain rules (decided 2026-10-19, step 2)

- **Fee 0 (free time):** payment is refused («nothing to pay»), the session stays `open`; exit closes
  it as `closed_free`. `/api/status` reports such a session as `unbezahlt` with `fee: 0`.
- **Paid = may leave:** a paid session closes at any later exit. No grace period, no extra fee.
- **Session history:** at most 50 sessions in RAM/flash. When full, the oldest closed one is dropped;
  open/paid sessions are never dropped.
- **Payment:** Karte abgelehnt changes nothing. Cash accumulates until the fee is covered, then
  change = inserted − fee. Card/TWINT after partial cash charges the rest. The amount booked is the
  fee at payment time. Transaction ref `TX-000001`, … from a counter stored with the sessions.
- **Registrations:** at most 50; name trimmed, 1–40 bytes; plate unique after normalization.
- **Formats:** duration always `<h> h <m> min` (hours unbounded, minutes rounded down): `0 h 5 min`,
  `26 h 3 min`. Europe/Zurich offset is computed by a pure function (EU rule), not via `TZ`.

Libraries: `lib/registration`, `lib/session` (store, state machine, payment), `lib/format`,
`lib/codec` (JSON files), `lib/kasse_api` (API handlers → status + body), `lib/kasse_view` (Kasse/Automat
page texts, CHF input parsing, pay-form handling, page HTML), `lib/html` (shared page building blocks),
`lib/barrier` (barrier state machine), `lib/gate` (entry/exit decisions on Pico 2), `lib/schranken_view`
(registration and camera pages).

File formats (LittleFS):
- `/registrations.json`: `{"registrations":[{"name":"Test Kunde A","kz":"LU123456"}]}`
- `/sessions.json`: `{"nextTx":2,"sessions":[{"kz":"LU123456","entry":1760525200,"exit":0,"amount":400,
  "cash":0,"method":"card","tx":"TX-000001","status":"paid"}]}` (method `none|card|twint|cash`,
  status `open|paid|closed|closed_free`). An invalid file is rejected as a whole.

## Code layout (decided)

One PlatformIO project:
- `[env:kasse]` and `[env:schranken]`: `board = rpipicow`, earlephilhower core, each with a
  `build_src_filter` selecting its own `src/` subfolder (e.g. `src/kasse/`, `src/schranken/`).
- `[env:native]` for host tests of `lib/` (tariff, plate, session state machine, change calculation).
- Libraries: ArduinoJson (`lib_deps`), core's WiFi, WebServer, HTTPClient, LittleFS, Servo.
- `lib/` stays free of Arduino headers. Hardware, WiFi, web server and storage glue live in `src/`.
- Hardware test suites run only on the board envs (`test_filter` / `test_ignore`).
