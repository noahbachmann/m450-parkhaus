# Architecture

How the system is built now. Mark new undecided items **(open)** and record the decision here when it's made.

## Hardware

- 2 × **Raspberry Pi Pico W** (WiFi needed) → PlatformIO `board = rpipicow`, earlephilhower core.
- Pico 2 drives 2 × SG90 servos (entry, exit) and reads 2 safety loops (push button or IR sensor).
  The loops are **optional hardware**: with nothing connected, `INPUT_PULLUP` reads HIGH = free, so the
  firmware runs unchanged without them. The loop pins are debounced (stable for 50 ms).
- Onboard LED on **both** Picos shows status: solid = ready, blinking = WLAN or NTP not ready.
  (On the Pico W the LED is on the CYW43 chip: `LED_BUILTIN` works through the core.)
- Pico 1 has no external hardware.

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

`pio run -e hwtest -t upload` flashes a wiring check (`src/hwtest/`): both servos move every 2 s, the console
shows the loops.

### WLAN credentials and IPs

- `include/secrets.h` (**gitignored**) holds SSID, password, the fixed IPs of both Picos (`KASSE_IP`,
  `SCHRANKEN_IP`, gateway, subnet) and `OPERATOR_PASSWORD`; the board build stops without it.
- `include/secrets.example.h` (committed) shows the format.
- Separate test WLAN (phone hotspot). It needs internet access for NTP.

## Responsibilities

| Pico 1 "Kasse" (env `kasse`) | Pico 2 "Schranken" (env `schranken`) |
|---|---|
| Parking sessions, **persisted in flash** | **All web pages**: camera simulation, Automat, Tarif, login; for the operator Kasse, Registrierung, Betrieb |
| Tariff calculation ([tariff.md](tariff.md)) | Registration list (name + plate), persistent |
| Payments by plate: card / TWINT / cash + change | Barrier control: 2 servos + safety loops (core 1) |
| JSON API for Pico 2 (status, entry, exit, pay, sessions, reset) | Event log: remote openings, safety stops, faults |
| `GET /` = link page to Pico 2 | Operator view, user «betrieb» |

The registration list lives on Pico 2 (Testkonzept test object 01). Sessions live on Pico 1; Pico 2 renders the
Kasse and Automat pages from `GET /api/sessions` and pays through `POST /api/pay`.

## Code layout

One PlatformIO project; envs `kasse`, `schranken`, `hwtest` (boards), `native` (tests), `sim_kasse`,
`sim_schranken` (PC simulator).

- `lib/` — all logic, no Arduino headers, native-tested. Time is a parameter.
- `src/kasse/`, `src/schranken/`, `src/common/` — board adapters (WebServer, HTTPClient, LittleFS, Servo, WLAN,
  NTP, LED). `src/sim/` — the same adapters for the PC (own socket HTTP server/client, files).
- **Routes exist only once, in `lib/`:** `routeKasse` (`lib/kasse_api`) for Pico 1 and `SchrankenApp`
  (`lib/schranken_app`) for Pico 2. Board and simulator mains only translate their web server into
  `WebRequest` / `WebResponse` (`lib/web`) and provide storage, clock and barriers.

Libraries:

| Library | Contents |
|---|---|
| `plate` | normalization, validation, invalid-plate text |
| `tariff` | `feeBreakdown`, `calculateFee` (integer Rappen) |
| `format` | CHF, Zurich date/time (EU DST rule, no `TZ`), durations, `trim`, `utf8Length` |
| `registration`, `session` | registration list; session store, state machine, payment (`payment.*`) |
| `codec` | JSON file formats |
| `event_log` | event log (max. 100) |
| `kasse_api` | Pico 1 API handlers + `routeKasse` |
| `web` | platform-neutral request/response, URL decoding |
| `html` | page skeleton, nav, notices |
| `auth` | login tokens and cookies |
| `gate` | entry/exit decisions on Pico 2, `KasseLink` (the HTTP link to Pico 1) |
| `barrier` | barrier state machine, `BarrierDriver` (core-1 side), `BarrierEventQueue`, `Debouncer` |
| `terminal` | call button, «Fahrzeug wendet», payment at the exit terminal |
| `kasse_client` | Pico 2 side of the Kasse: session list, payments, reset through the API |
| `kasse_view` | Kasse/Automat/Tarif texts and pages |
| `schranken_view`, `operator_view` | camera, registration, login pages; operator view, remote opening, logging |
| `schranken_app` | Pico 2 app: route table, login check, notices, saving; `BarrierPort`, `AppPlatform`, `LocalBarrier` |
| `testdata` | test-data defaults (KZ table) |
| `test_support` | header-only test fixtures and the system-test rig (not in the firmware) |

### Pico 2 app (`lib/schranken_app`)

- One `ROUTES[]` table `{method, path, admin, handler}`. `admin` routes redirect to `/login?next=…` without a
  valid cookie; the same table is the whitelist for `next` (admin GET pages, default `/betrieb`). An unknown
  method/path is 404 for everyone.
- After every handler the app saves what changed: `RegistrationList::revision()` and `EventLog::revision()`
  are compared before and after (works with a full log too).
- One-time notices after a POST: redirect to `<page>?m=<id>`, the page shows the notice only for that id.
- Ports:
  - `BarrierPort` (state, loop fault, loop held, `requestOpen`, `requestPass` → `vehiclePassed()` result,
    `setLoopHeld`).
  - `AppPlatform` (read/write file, random words for tokens, log line).
- **Board (`src/schranken/lane.*`):** `CoreLane` runs servo + loop on **core 1** (`setup1`/`loop1`), so a blocking
  HTTP call on core 0 never delays a safety stop. Core 0 → core 1: open/pass request counters and the held flag;
  core 1 → core 0: state, fault, pass acknowledgement, events through a lock-free queue (8 entries, drops are
  counted). `requestPass` waits up to 20 ms for core 1's answer. Core 0 logs and saves the events; core 1 never
  touches LittleFS or `Serial`.
- **Simulator:** `LocalBarrier` (no loop pin; the held flag is the loop), updated every ≤ 20 ms.

## Flows

**Entry** (Pico 2): normalize plate → validate → registered? (no → stay closed, «… nicht registriert, bitte
Ruftaste drücken») → `POST /api/entry` (optional backdated entry time) → Pico 1 rejects if a session is already
open («Parkvorgang bereits offen – bitte Ruftaste drücken») → success → barrier opens in the same request.
Pico 1 unreachable → stay closed, «Kasse nicht erreichbar – bitte Ruftaste drücken», logged, **no buffered
session**.

**Barrier:** open → closing once `BARRIER_MIN_OPEN_MS` (5 s) has passed, the vehicle has passed («Durchfahren»,
or the loop going occupied → free) **and** the loop is free. Closing = 2 s sweep 90° → 0°. Loop occupied while
closing → stop and fully open in the same update, event «Sicherheitsabschaltung», minimum open time restarts.
Loop occupied 5 min → fault «Sicherungsschleife <Einfahrt|Ausfahrt> länger als 5 min belegt», once per
occupation, barrier stays open. Loop occupied at boot → barrier starts open. «Fahrzeug bleibt stehen» /
«Fahrzeug fährt weiter» (`POST /kamera/schleife`) holds the loop from the web page (ORed with the pin), only
while the barrier is up; freeing it counts as the pass.

**Payment** (Pico 1, from the Automat, the Kasse or the exit terminal): open session by plate → fee → simulated
method → paid with method, amount and transaction reference. Cash accumulates until the fee is covered, then
change = inserted − fee. Card/TWINT after partial cash charges only the rest; the session books the whole fee
(method card/TWINT, the cash part kept). Already paid → «bereits bezahlt». No open session → «Kein offener
Parkvorgang für <PLATE>».

**Exit** (Pico 2): normalize → `POST /api/exit` → closed (paid) or closed_free (fee 0) → barrier opens, the clock
stops. Unpaid → closed, «Gebühr CHF x.xx nicht bezahlt – bitte am Automaten bezahlen», payment offered at the
exit terminal (card/TWINT). No session → «Kein Parkvorgang gefunden – bitte Ruftaste drücken». Timeout/error →
closed, «Kasse nicht erreichbar – bitte Ruftaste drücken». **Fail-safe: anything unexpected = closed.**

## Web pages (all on Pico 2)

Server-rendered HTML forms, no JavaScript. Nav: «Kamerasimulation», «Automat», «Tarif» for everyone; «Kasse»,
«Registrierung», «Betrieb» after login; right side «🔒 Anmelden» or «👤 betrieb (Admin)» + «Abmelden».

| Route | Access | Contents |
|---|---|---|
| `GET /` | all | 303 to `/kamera` |
| `GET /kamera` | all | per lane: barrier state, terminal text, plate field (+ optional `datetime-local` entry time at the entry), «Erfassen», «Durchfahren», «Fahrzeug bleibt stehen», «Ruftaste»; exit: «Fahrzeug wendet» and the payment form; below all sessions (testing aid) and plate suggestions |
| `POST /kamera/einfahrt`, `/kamera/ausfahrt`, `/kamera/durchfahren`, `/kamera/schleife`, `/kamera/ruftaste`, `/kamera/abbrechen`, `/kamera/bezahlen` | all | the actions above (field `kz`, `entry`, `lane`, `held`, `method`), 303 to `/kamera` |
| `GET /automat?kz=`, `POST /automat/pay` | all | Automat: tariff, search by plate (**active session only**, nothing about other cars), pay with Karte OK / Karte abgelehnt / TWINT / Bargeld (CHF amount) |
| `GET /tarif` | all | tariff sentence and example fees from `lib/tariff` |
| `GET /login`, `POST /login`, `POST /logout` | all | login form (`user`, `password`, `next`); cookie `parkhaus_session` (`HttpOnly; SameSite=Strict`), 128-bit random token, max. 4 logins in RAM |
| `GET /kasse?kz=`, `POST /kasse/pay` | operator | Kasse: any plate (latest session, also closed), pay, list of all sessions |
| `GET /registrierung`, `POST /registrierung/add`, `/registrierung/delete` | operator | registrations |
| `GET /betrieb?filter=`, `POST /betrieb/oeffnen` | operator | lanes (barrier, last plate, payment status, call, fault), remote opening with reason, event log with type filter, test-data reset |
| `POST /reset` | operator | test-data reset: default registrations, empty log, Pico 1 `/api/reset` |

Remote opening: reason «Kennzeichen nicht erkannt», «Kulanz» or «Technische Störung» (else «Bitte Grund
auswählen»); at the entry also «Neuer Kunde registrieren» (+ name: registers the plate, then opens). Opens only
when Pico 1 confirms (entry: session opened or already open; exit: closed, unpaid → «ohne Zahlung ausgefahren»,
or no session). Pico 1 not answering → stays closed, logged «Kasse nicht erreichbar».

## Interface Pico 2 → Pico 1 (OP-01)

JSON over HTTP on Pico 1, port 80. Requests with a body use `Content-Type: application/json`.
All times are Unix epoch seconds, amounts integer **Rappen**. Plates may be sent unnormalized: Pico 1 normalizes
and validates them. Every error is `{"error":"<code>"}` with the given HTTP status. A body that is not a JSON object
with a string `kz` → 400 `bad_request`. Pico 2 client timeout: **1000 ms**; timeout or connection error = Pico 1
unreachable. **POST routes only from `SCHRANKEN_IP`** (others: 403 `forbidden`); GET routes are open.

- **`GET /api/status?kz=<plate>`** (TF-29) — 200 `{"kz","status":"bezahlt"|"unbezahlt"|"unbekannt","fee","entry"}`
  (`unbekannt` = no open session, without `fee`/`entry`) · 400 `invalid_plate` · 503 `time_not_synced` if the
  clock is before an open session's entry.
- **`POST /api/entry`** `{"kz"}` or `{"kz","entry":<epoch>}` (backdated) — 201 `{"kz","entry"}`, also for a repeated
  entry (see domain rules) · 409 `already_open` · 400 `invalid_plate` / `entry_in_future` · 503 `time_not_synced` /
  `store_full`.
- **`POST /api/exit`** `{"kz"}`; operator: `{"kz","override":true}` — 200 `{"kz","status":"closed"|"closed_free"|"closed_unpaid","exit","fee"}` ·
  402 `{"error":"unpaid","fee"}` · 404 `no_session` · 400 `invalid_plate` · 503 `time_not_synced`.
- **`POST /api/pay`** `{"kz","method":"card_ok"|"card_declined"|"twint"}` or `{"kz","method":"cash","amount":<Rappen>}`
  (1 … 100000) — 200 `{"kz","status":"bezahlt","amount","method":"card"|"twint"|"cash","tx"}` + `change` (cash) or
  `cash` (card/TWINT after partial cash: the cash part) · 200 `{"kz","status":"unbezahlt","amount","remaining"}`
  (partial cash) · 402 `declined` · 409 `already_paid` / `nothing_to_pay` · 404 `no_session` · 400
  `invalid_plate` / `invalid_amount` / `bad_request` · 503 `time_not_synced`.
- **`GET /api/sessions`** — the `/sessions.json` format plus `"now"` (Pico 1 time) and `"synced"`.
- **`POST /api/reset`** — no sessions, tx counter 1; 200 `{"status":"reset"}`.

An exit attempt without success (or abort) changes nothing: the session stays open and the clock keeps running.

## Data

- **Plate:** normalize = uppercase, remove all whitespace. Valid = `^[A-Z]{2}[0-9]{1,6}$` after normalization.
- **Registration:** name (trimmed, 1–40 characters), normalized plate (unique); at most 50.
- **Parking session:** plate, entry, exit, amount, cash inserted, payment method, transaction ref, status
  `open` → `paid` → `closed`; also `closed_free` («abgeschlossen (Freizeit)») and `closed_unpaid` («ohne Zahlung
  ausgefahren»). At most one open/paid session per plate; at most 50 sessions (oldest closed one dropped).
- **Time:** NTP at boot on both Picos. Unix epoch seconds (UTC) internally, shown in Europe/Zurich as
  `TT.MM.JJJJ HH:MM:SS`. **Pico 1 stamps entry and exit times**; Pico 2 sends a time only for a backdated entry.
  Without NTP, entry/exit/payment are rejected and the LED blinks.
- **Storage:** LittleFS + JSON; data in RAM, loaded at boot; on every change written to `<name>.tmp`, then renamed
  over `<name>.json` (survives a power cut; the simulator does the same). Pico 1: `/sessions.json`. Pico 2:
  `/registrations.json`, `/events.json`. An invalid file is rejected as a whole and replaced by the defaults.
  - `/registrations.json`: `{"registrations":[{"name":"Test Kunde A","kz":"LU123456"}]}`
  - `/sessions.json`: `{"nextTx":2,"sessions":[{"kz":"LU123456","entry":1760525200,"exit":0,"amount":400,"cash":0,"method":"card","tx":"TX-000001","status":"paid"}]}`
    (method `none|card|twint|cash`); on load `nextTx` is raised above every stored `TX-` number.
  - `/events.json`: `{"events":[{"t":1760530000,"type":"remote_opening","lane":"entry","kz":"BE4711","user":"betrieb","reason":"Kennzeichen nicht erkannt","note":"Parkvorgang eröffnet"}]}`
    (type `safety_stop|loop_fault|remote_opening|kasse_unreachable|call_button|payment_alert`).

## Domain rules

- **Fee 0 (free time):** payment is refused («nothing to pay»), the session stays `open`; exit closes it as
  `closed_free`. `/api/status` reports it as `unbezahlt` with `fee: 0`.
- **Repeated entry:** if Pico 1 answers an entry only after Pico 2's 1000 ms timeout, the session exists although
  the barrier stayed closed. A retry for the same plate succeeds (201, nothing changes) if the session is open and
  unpaid (no cash inserted) and it is the same entry: without backdating opened less than 30 s ago
  (`REPEAT_ENTRY_SECONDS`), with backdating the same entry time. Otherwise `already_open` (TF-10).
- **Paid = may leave:** a paid session closes at any later exit. No grace period, no extra fee.
- **Payment:** «Karte abgelehnt» changes nothing. The amount booked is the fee at payment time. Transaction ref
  `TX-000001`, … from a counter stored with the sessions. 5 declines in a row at the exit terminal → «Zahlung
  abgelehnt – Leitstelle wurde benachrichtigt», event «Zahlung 5× abgelehnt», call in the operator view.
- **Formats:** duration `<h> h <m> min` (hours unbounded, minutes rounded down).

## PC simulator

Two native programs with the same `lib/` code, talking HTTP like the boards:
- `src/sim/kasse/` (env `sim_kasse`, `localhost:8081`): Pico 1 through `routeKasse` (always synced, no client
  check).
- `src/sim/schranken/` (env `sim_schranken`, `localhost:8082`): Pico 2 through `SchrankenApp` with two
  `LocalBarrier`s; calls the Kasse at `KASSE_URL` (default `http://localhost:8081`), 1000 ms timeout.
- `src/sim/common/`: minimal HTTP server (Cookie / Set-Cookie) + client (Winsock/POSIX), data files.
- Config by environment: `SIM_HOST` (default `127.0.0.1`), `SIM_DATA` (default `sim-data`), `KASSE_URL`,
  `SCHRANKEN_PUBLIC_URL` (link on Pico 1's `/`), `OPERATOR_PASSWORD` (default `parkhaus`).
- Docker: `Dockerfile` (g++ 13, ArduinoJson 7.4.3 single header with checksum, static libstdc++; runtime
  `debian:bookworm-slim`), `docker-compose.yml` with `kasse` (builds the image) and `schranken`, ports bound to
  127.0.0.1, data in `./sim-data/<service>`. Pico 1 off = `docker compose stop kasse`.

## Tests

- Unit suites per library in `test/native/test_<name>/` (Unity, `pio test -e native`).
- System test through the real routes (`lib/test_support/system_rig.h`): `test_system_entry` (Testkonzept 8.1–8.2),
  `test_system_safety` (8.3–8.4), `test_system_payment` (8.5–8.7). A "reboot" rebuilds a Pico from its files.
- No board test suites (`test_ignore = *` on the board envs).
