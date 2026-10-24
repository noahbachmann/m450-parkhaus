# Test concept (Testkonzept v1.0, 25.09.2026)

Source: `Testkonzept_Parkhaus_Pico_v1.pdf` (HERMES template, 34 pages, untracked in the repo root). Authors Bachmann/Willimann,
approval by teacher pending. Test lead: N. Bachmann. Defects → git issues (labels = defect class).

Abbreviations: TF = test case, KZ = test plate, AK = acceptance criterion, R = tariff reference value,
ZM = payment method, TU = test environment, OP = open point.

## Test goals

| # | Goal | Metric | Prio |
|---|---|---|---|
| 01 | Entry opens only for registered plates | 0 unauthorized openings | 1 |
| 02 | Entry opens fast | 10/10 measurements **≤ 2.0 s** from confirming plate input to arm fully open | 1 |
| 03 | Exit opens only if paid | 0 openings without payment (except logged remote opening) | 1 |
| 04 | Fee correct | CHF 0.00 deviation for R-01..R-11; R-12 rejected | 1 |
| 05 | Pico 2 → Pico 1 status query correct | 20/20 correct; on failure barrier stays closed | 1 |
| 06 | Payments booked reliably | status «bezahlt» **within 3 s** (TF-20, TF-21) | 1 |
| 07 | Barrier endangers nobody | 0 closing movements with occupied loop | 1 |
| 08 | Registration stores only valid, unique plates | all invalid/duplicates rejected | 2 |
| 09 | Remote openings and safety stops traceable | 100 % logged | 2 |
| 10 | UIs usable without instruction | stranger pays in < 2 min | 3 |

## Strategy

- Risk-based: biggest risks = barrier opens without authorization/payment, barrier closes on a car,
  wrong fee.
- Black-box from ACs: equivalence classes (valid/invalid plates; paid/unpaid/unknown), boundary values
  (tariff, time limits), state transitions (session: open → paid → closed), plus one exploratory session.
- **Levels:** developer tests = tariff calculation and plate validation as C++ modules with Unity via
  `pio test` (host, to the second) → integration (Pico 2 ↔ Pico 1, Pico ↔ servo/loop) → system
  (web UIs, full setup; main focus) → acceptance (TF-32 + all prio-1 cases in front of the teacher).
- **Workarounds for the prototype:** plates typed into the Pico 2 web UI (no camera); safety loop =
  push button or IR sensor; payments simulated at Kasse; **entry time can be entered at the entry form**
  (empty = now) to simulate long stays; 95 % recognition rate replaced by normalization test TF-06.
- **Regression:** after each fix, rerun the affected TF + smoke test **TF-01, TF-04, TF-20, TF-23, TF-24**.
  Rerun TF-32 before acceptance.
- Start criteria: concept approved, prerequisites met, OP-01 resolved, `pio test` dev tests green,
  entry test TF-04 + TF-20 passed.
- Abort: class-4 defect blocks tests, a Pico keeps rebooting / unreachable, wrong test data,
  > 30 % of a day's TFs fail, danger to people/hardware. Resume with TF-04 + TF-20.
- End: all prio 1 + 2 executed, all prio 1 passed, no open class 3/4 defects, test report written.

## Defect classes

| Class | Meaning | Project examples |
|---|---|---|
| 0 | defect-free | — |
| 1 | trivial | typo, inconsistent amount/date format, ugly layout |
| 2 | minor | error message missing/unclear, terminal feedback late, 5-min fault message missing |
| 3 | major | wrong fee, payment not booked, opening time > 2 s, remote opening not logged |
| 4 | critical | barrier opens without payment or for unregistered plate, closes on occupied loop, **Pico crashes on an input** |

Only classes 1–2 → conditional acceptance. Any class 3–4 → no acceptance.

## Test objects

01 registration UI (Pico 2: add, list, delete plates) · 02 entry barrier (servo, terminal feedback,
opens session; Pico 1 unreachable → reject, nothing buffered) · 03 exit barrier (status query, closes
session) · 04 camera simulation (plate fields for entry/exit, optional entry time, call button, remote
opening) · 05 safety loop + logic · 06 Kasse UI (Pico 1: search plate, show duration and fee, record payment)
· 07 Automat view (Pico 1: pay by plate with card/TWINT/cash + change) · 08 tariff calculation (Pico 1)
· 09 payment-status interface (HTTP, Pico 2 → Pico 1: bezahlt / unbezahlt / unbekannt) · 10 parking
sessions (plate, entry, exit, amount, payment method, status) · 11 event log (Pico 2: remote openings,
safety stops, faults).

## UI formats (apply everywhere)

- Plate normal form: uppercase, no spaces (`lu 123 456` → `LU123456`). Valid: **2 letters + 1–6 digits**.
- Date/time: `TT.MM.JJJJ HH:MM:SS` (dd.mm.yyyy). Duration e.g. `1 h 20 min` (also `0 h 5 min`, `26 h 3 min`). Amounts: `CHF 0.00`.
- Session statuses: «offen», «bezahlt», «abgeschlossen», «abgeschlossen (Freizeit)»,
  «ohne Zahlung ausgefahren». Payment status: «bezahlt» / «unbezahlt» / «unbekannt».

## Test cases

Base precondition for all: both Picos up and on test WLAN, clock synced, test data loaded.
Strings in «» are the exact expected German UI text.

### Registration
| TF | Prio | Ref | Setup → action | Expected |
|---|---|---|---|---|
| 01 | 1 | US-01 AK2 | Register name «Test Kunde C» + KZ-12 «NW 654321»; reload; reboot Pico 2 | «Kennzeichen registriert»; listed as `NW654321`; **survives reboot** |
| 02 | 2 | US-01 AK2 | Try KZ-05 (empty), KZ-06, KZ-07, KZ-09; then KZ-13 «UR 1» | Clear error for each invalid, nothing stored, no crash; KZ-13 stored |
| 03 | 2 | US-01 AK4 | KZ-01 belongs to Kunde A; register «LU 123456» and «lu123456» for Kunde B | Both rejected: «Kennzeichen bereits registriert»; KZ-01 once, still Kunde A |

### Entry and parking session
| TF | Prio | Ref | Setup → action | Expected |
|---|---|---|---|---|
| 04 | 1 | US-06 AK1/2 | Camera sim «Einfahrt», enter KZ-01, «Erfassen»; press + release loop | Opens fully **within 2 s** without further input; closes after loop released |
| 05 | 2 | US-06 AK4 | KZ-01 at entry; drive through; KZ-03 at entry | «LU123456 – Willkommen, Einfahrt frei»; «BE4711 – nicht registriert, bitte Ruftaste drücken»; text shown with/before barrier moves |
| 06 | 2 | US-06 AK3 (subst.) | Entry «lu 123 456»; pay at Kasse; exit «` LU123456 `» (leading/trailing spaces) | Opens; exactly one session on `LU123456`; exit matched to it and opens |
| 07 | 1 | US-06 AK2 | KZ-03 (unregistered) at entry, watch 10 s, search at Kasse | Stays closed, refers to call button; no session |
| 08 | 1 | US-06 AK2 | 10 entry cycles (KZ-01/KZ-02 alternating, pay + exit between), 60 fps video | All 10 ≤ 2.0 s; mean + max recorded |
| 09 | 1 | US-07 AK1/2 | KZ-02 at entry, note reference time (time.is), check Kasse, reload after 30 s | Open session `ZH98765`; entry time `TT.MM.JJJJ HH:MM:SS`, **±2 s**; duration grew ~30 s |
| 10 | 2 | US-07 AK4 | KZ-02 already has open session; enter again | Stays closed; «Parkvorgang bereits offen – bitte Ruftaste drücken»; still one session, entry time unchanged |
| 11 | 2 | US-07 AK5 | Power off Pico 1; KZ-01 at entry; after 2 min power on; retry | Without Pico 1: closed, understandable fault message, no session. After: opens, session with 2nd attempt's time (±2 s) |

### Barrier safety
| TF | Prio | Ref | Setup → action | Expected |
|---|---|---|---|---|
| 12 | 1 | US-09 AK1 | Barrier open, hold loop 30 s, release; repeat at exit | Stays fully open while occupied; closes only after release; same at both |
| 13 | 1 | US-09 AK2 | Closing takes ~2 s; trigger loop during closing; 3× per barrier | Stops **< 0.5 s** and goes fully up in all 6 runs; closes normally after release |
| 14 | 2 | US-09 AK4 | After TF-13, open event log, filter «Sicherheitsabschaltung» | Exactly 6 entries with barrier (Einfahrt/Ausfahrt) + timestamp (±2 s) |
| 15 | 2 | US-09 AK3 | Keep loop occupied; check at 4:50 and 5:10 min | 4:50 none; 5:10 «Sicherungsschleife Einfahrt länger als 5 min belegt» in UI + log; barrier stays open |

### Remote opening (user «betrieb» logged in)
| TF | Prio | Ref | Setup → action | Expected |
|---|---|---|---|---|
| 16 | 2 | US-08 AK2–5 | KZ-03 at entry (closed); «Ruftaste»; operator view; «Schranke öffnen» with reason «Kennzeichen nicht erkannt» | View shows lane «Einfahrt», plate «BE4711», status «kein Parkvorgang»; opens; session opened for KZ-03; log: user, time, lane, reason |
| 17 | 2 | US-08 AK3 | «Schranke öffnen» → «Bestätigen» without reason | Stays closed, «Bitte Grund auswählen», nothing logged |

### Fee and payment
| TF | Prio | Ref | Setup → action | Expected |
|---|---|---|---|---|
| 18 | 1 | US-14 AK2 | For each R-01..R-12: KZ-01 entry with backdated time = system-test value; read Kasse within 60 s; close/reset | R-01..R-11 exact; R-12 error, no fee; format `CHF 0.00` |
| 19 | 2 | US-14 AK1–3 | Session 1 h 20 min; Kasse search; open «Tarif»; same in Automat | Plate, entry time, «1 h 20 min», «CHF 4.00», note 2 started hours + 15 free minutes; tariff viewable in both, equals tariff.md. |
| 20 | 1 | Auftrag | KZ-01 open, CHF 4.00; Kasse: «Karte OK», «Bezahlen»; pay again | Within 3 s «Bezahlt – CHF 4.00, Karte» + **transaction reference**; second attempt: «bereits bezahlt», no double booking |
| 21 | 1 | US-17 AK1/2 | KZ-01 + KZ-02 CHF 6.00, KZ-08 CHF 2.00. Automat: KZ-01 cash CHF 10.00; KZ-02 TWINT; KZ-08 cash 1.00 then 1.00 | KZ-01 change CHF 4.00, «bezahlt» ≤ 3 s; KZ-02 «bezahlt» ≤ 3 s; KZ-08 «Restbetrag CHF 1.00» + «unbezahlt», then «bezahlt», change CHF 0.00 |
| 22 | 2 | US-17 AK1 neg. | Automat, KZ-10 (no session), try to pay | «Kein offener Parkvorgang für SZ555», no payment offered, no booking |

### Exit and interface
| TF | Prio | Ref | Setup → action | Expected |
|---|---|---|---|---|
| 23 | 1 | US-13 AK1/2, US-16 AK1 | KZ-01 paid; camera sim «Ausfahrt», enter, note time; check Kasse now and after 1 min | Opens within 2 s; session «abgeschlossen», exit time ±2 s; duration no longer changes |
| 24 | 1 | US-16 AK1/2 | KZ-02 unpaid (> CHF 0.00) at exit; watch 10 s; pay at Automat; retry | Closed; «Gebühr CHF x.xx nicht bezahlt – bitte am Automaten bezahlen»; after payment opens |
| 25 | 2 | US-14 AK2 | KZ-08 entered 5 min ago; exit | Opens; session CHF 0.00, «abgeschlossen (Freizeit)» |
| 26 | 2 | US-13 AK3 | KZ-10 (no session) at exit | Closed; «Kein Parkvorgang gefunden – bitte Ruftaste drücken» |
| 27 | 2 | US-13 AK4 | KZ-02 unpaid, 58 min; exit (closed); «Fahrzeug wendet»/«Abbrechen»; wait past 60 min | Session stays «offen», no exit time; amount CHF 2.00 → CHF 4.00 after 60 min |
| 28 | 2 | US-16 AK2–4 | Exit terminal: KZ-01 «Karte abgelehnt» then «TWINT»; KZ-02 5× «Karte abgelehnt» | KZ-01: «Zahlung abgelehnt – anderes Zahlungsmittel wählen», closed; opens after TWINT. KZ-02 after 5th: «Leitstelle wurde benachrichtigt» + operator view entry (lane, plate); stays closed |
| 29 | 1 | Auftrag | curl the status endpoint on Pico 1 (e.g. `GET /api/status?kz=LU123456`) for KZ-01/02/10, empty, «LU-12#45»; 20 requests total | «bezahlt» / «unbezahlt» / «unbekannt»; empty/invalid → error (e.g. HTTP 400), no crash; all correct, each **< 1 s** |
| 30 | 1 | US-16 AK1 fail-safe | Pico 1 off; KZ-02 then KZ-01 (paid) at exit; Pico 1 on; KZ-01 again | Both closed: «Kasse nicht erreichbar – bitte Ruftaste drücken», fault logged. After reboot KZ-01 still paid (**persisted**), opens |
| 31 | 2 | US-16 AK5, US-08 AK3/5 | KZ-02 unpaid at exit; operator opens with reason «Kulanz» | Opens; log: time, user «betrieb», lane «Ausfahrt», plate, reason «Kulanz», note «ohne Zahlung»; session «ohne Zahlung ausgefahren» |

### Whole system
| TF | Prio | Ref | Setup → action | Expected |
|---|---|---|---|---|
| 32 | 1 | Auftrag, acceptance | Register KZ-11 «OW 2026»; entry backdated 2 h 30 min; exit (unpaid); Automat cash CHF 10.00; exit again | Stored; entry opens; fee CHF 6.00; 1st exit closed; change CHF 4.00; 2nd exit opens, session closed |
| 33 | 3 | Goal 10, US-14 AK3 | Stranger told only «Bezahle das Parken für das Auto ZH 98765.» | Pays without help in < 2 min; unclear points logged as class 1/2 |
| 34 | 3 | all | 45-min exploratory charter: simultaneous entry/exit, double-click «Bezahlen», pay during exit query, browser back, two browsers on Kasse | Session documented, defects classified and filed |

## Test data (reset before each test day)

### Plates
| ID | Plate | State | Use |
|---|---|---|---|
| KZ-01 | LU 123456 | registered (Test Kunde A) | normal case |
| KZ-02 | ZH 98765 | registered (Test Kunde B) | normal, unpaid cases |
| KZ-03 | BE 4711 | not registered | negative entry |
| KZ-04 | lu 123 456 | spelling variant of KZ-01 | normalization |
| KZ-05 | (empty) | invalid | negative input |
| KZ-06 | LU-12#45 | invalid (special chars) | negative input |
| KZ-07 | LU 1234567 | invalid (7 digits) | boundary above |
| KZ-08 | AG 1 | registered | shortest plate (pre-registered) |
| KZ-09 | L 123 | invalid (1 letter) | boundary canton code |
| KZ-10 | SZ 555 | registered, no session | negative exit/Automat |
| KZ-11 | OW 2026 | initially not registered | TF-32 |
| KZ-12 | NW 654321 | not registered | TF-01 |
| KZ-13 | UR 1 | not registered | shortest valid, TF-02 |

Operator test user: «betrieb» (role Betriebspersonal), for TF-16/17/31.

### Simulated payment methods
| ID | Method | Behaviour |
|---|---|---|
| ZM-01 | Karte OK | confirmed |
| ZM-02 | Karte abgelehnt | rejected with reason «abgelehnt» |
| ZM-03 | TWINT | confirmed |
| ZM-04 | Bargeld | enter inserted CHF amount, change calculated (partial amounts accumulate) |

Tariff reference values R-01..R-12: see [tariff.md](tariff.md).

## Environment

- **TU-1** dev laptop, `pio test`, no hardware (tariff + plate validation).
- **TU-2** full table-top setup, own test WLAN (e.g. phone hotspot), firmware with camera + Kasse simulation.
- **TU-3** acceptance: same as TU-2, frozen firmware tag **`v1.0`**, fresh test data.
- Setup: 2 servos with arms, 1 button/IR sensor per barrier as loop, both Pico W on breadboards,
  **fixed IPs**, each Pico on its own switchable USB 5 V supply (TF-11, TF-30), laptop with browser,
  curl, `pio device monitor`. A **test-data reset** mechanism is expected (reset plates and sessions).
- Tools: Excel test protocol, git issues, phone video (TF-08/13/20/23), serial console.

## Deviations of the implementation

The PDF is the spec; the implementation differs here (also listed in the README for the teacher):

- **All pages on Pico 2.** Pico 1 only serves the JSON API (+ a link page). The Kasse page (test object 06)
  is on Pico 2 and needs the operator login; the Automat (test object 07) is on Pico 2 and public.
- **Payment at the exit terminal** (card/TWINT) in addition to the Automat (US-16 AK3, used by TF-28).
- **TF-10:** a second entry within 30 s for an open, unpaid session counts as the same entry and opens (Pico 1
  may have answered after Pico 2's 1000 ms timeout). TF-10 therefore waits ≥ 30 s between the two entries.
- **Safety loop in the simulator:** «Fahrzeug bleibt stehen» / «Fahrzeug fährt weiter» on the camera page; on
  the board ORed with the loop pin. «Durchfahren» stands in for a car passing without a loop.
- **Writes on Pico 1** (POST /api/*) are accepted only from Pico 2's IP; GET /api/status stays open (TF-29).
- **Test environment:** without hardware, the system tests run as native tests (TU-1) through the same routes
  and forms as the browser (`test/native/test_system_*`); timing goals (≤ 2 s, ≤ 3 s, < 1 s) are not measured.

## Open points

- **OP-01:** resolved, see the interface section in architecture.md.
- **OP-02:** submission date not yet known.

Test plan dates: dev tests 15.10–05.11.2026; system/integration 05.11 / 12.11.2026; TF-32 + TF-34
19.11.2026; fixes + regression 19.–26.11.2026; acceptance date open.
