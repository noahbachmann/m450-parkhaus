# Requirements — user stories US-01..22

Source: `Anforderungen_UserStories.pdf` (Bachmann/Willimann, 07.09.2026, untracked in the repo root).
Written for a *real* car park with cameras, app, sensors and payment provider. The prototype implements only part of it.
Value = Nutzen (H/M/L), SP = story points (Fibonacci). Total 126 SP.

**Status legend:** `done` = implemented and covered by the native tests (unit + system) and the PC simulator, not
run on hardware · `out` = not buildable/testable with the Pico setup (reason given; same scope as Testkonzept
chapter 2).

Prototype substitutions that apply everywhere: "camera" = plate typed into Pico 2 web UI;
"app" does not exist; "Terminal" = the camera-simulation page; "Leitstelle"/operator = operator view
on Pico 2 (user «betrieb»); payments are simulated.

## A — Registration and planning

**US-01 Registration via app or desktop** — customer · H · 8
*As a customer I want to register by phone or desktop so I'm let into the car park.*
1. Registration via app and web browser with the same account. — `out` (no app; web UI only)
2. Name, e-mail and at least one plate are captured. — `done` (prototype: name + plate)
3. E-mail verified via confirmation link; inactive until confirmed. — `out` (no mail)
4. A plate can't belong to two active accounts at once. — `done`

**US-02 Quick registration at entry barrier (QR code)** — new customer · H · 5 — `out`
Unknown plates would get in and register via QR later. Contradicts the assignment (entry only with
a registered plate).

**US-03 Occupancy sensors + free-space display** — M · 8 — `out` (no sensors/LEDs/board)
**US-04 See free spaces in the app** — M · 3 — `out` (no app)
**US-05 Filter spaces by type** — L · 5 — `out` (no app)

## B — Entry and parking

**US-06 Entry barrier opens automatically** — customer · H · 13
*As a customer I want the entry barrier to open automatically when my plate is recognized.*
1. Plate captured without driver action. — `done` (typed into the camera simulation)
2. Plate linked to an active account → barrier opens **within 2 s**. — `done`
3. Recognition rate ≥ 95 %. — `out`, replaced by plate normalization check (TF-06) → normalization is `done`
4. Terminal shows visible feedback with the recognized plate. — `done`

**US-07 Parking clock starts automatically at entry** — customer · H · 3
1. Entry opens a parking session with plate and **to-the-second timestamp**. — `done`
2. Starts without customer input. — `done`
3. Running duration visible in the app within 30 s. — `out` (checked at Kasse instead)
4. **At most one open session per plate.** — `done`
5. Timestamp generated locally, stays correct on connection loss. — `done` (prototype: Pico 1 unreachable
   → entry rejected, no buffered session; a retry within 30 s after a late answer counts as the same entry)

**US-08 Help when plate not recognized** — customer / operator · H · 8 — `done`
1. Call button at both terminals opens voice link to control room. — `out` (button only shows a message)
2. Operator sees lane, last recognized plate and payment status.
3. Operator can open the barrier remotely and **must pick a reason from a list**.
4. Manual opening at entry still opens a parking session.
5. Every remote opening logged with user, time, lane, reason.

**US-09 Safe barrier movement** — customer / operator · H · 5 — `done`
1. Barrier closes only when the safety loop behind it reports free.
2. Object detected while closing → stop immediately, arm goes fully up.
3. Loop occupied > 5 min → fault message (US-21).
4. All safety shutdowns logged with timestamp.
5. SN EN 12453 conformity proof. — `out`

**US-10 Signposts and plans** — H · 3 — `out` (building measure)
**US-11 Numbered parking spaces** — M · 3 — `out` (building / app)
**US-12 Disabled parking spaces** — H · 3 — `out` (building / app)

## C — Exit and payment

**US-13 Automatic recognition at exit** — customer · H · 8
1. Plate captured at exit, matching open session determined. — `done`
2. Session ends at recognition time; clock stops. — `done`
3. No open session for the plate → barrier stays closed, customer referred to call button. — `done`
4. Aborted (car turns around) → session stays open, clock keeps running. — `done`

**US-14 Understand duration and cost** — customer · H · 5
1. Running duration and amount shown (app). — `done` at Kasse and Automat; app `out`
2. Calculated by the tariff; started time units and free minutes are shown. — `done`
3. Valid tariff viewable (app and terminal). — `done` (/tarif, linked from Kasse and Automat)

**US-15 Stored payment method charged automatically** — H · 8 — `out` (no app, no payment provider)

**US-16 Prevent exit on failed payment** — operator · H · 3
1. Barrier opens **only** on confirmed payment (or fee CHF 0.00). — `done`
2. Payment declined → barrier stays closed, reason shown clearly. — `done`
3. Customer can use another payment method directly at the exit terminal. — `done`
4. After 5 failed attempts the control room is involved automatically. — `done`
5. Every opening without paid session logged with reason and user. — `done`

**US-17 Pay at machine via plate** — customer · H · 8 — `done` (public Automat page on Pico 2)
1. Card, TWINT and cash in CHF; cash gives change.
2. After successful payment, session marked paid **within 3 s** and exit possible.

**US-18 Receipt (PDF, printed)** — M · 3 — `out` (no app / printer)

## D — Operation and safety

**US-19 Configure tariffs** — H · 8 — `out` (tariff fixed in code, see [tariff.md](tariff.md))
**US-20 Utilization and revenue reports** — M · 5 — `out`
**US-21 Automatic fault reporting** (e-mail/SMS, back office) — H · 8 — `out` as specified; faults are
only shown in the web UI and event log (TF-15, TF-30).
**US-22 Exit on power failure / fire alarm** — H · 3 — `out` (no fire alarm; unpowered servo can't open)

## Summary

`done`: US-01 AK2/4 · US-06 AK1/2/4 (+ normalization for AK3) · US-07 AK1/2/4/5 · US-08 AK2–5 · US-09 AK1–4 ·
US-13 AK1–4 · US-14 AK1 (Kasse/Automat)/AK2/AK3 · US-16 AK1–5 · US-17 AK1/2 — plus the assignment items
(barriers, fee, payment). Everything else is `out` (see the reasons above).
