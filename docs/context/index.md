# Agent context — index

Condensed, English version of the three source PDFs (kept untracked in the repo root and ignored by git:
`A01-03-umsetzung-parksystem.pdf`, `Anforderungen_UserStories.pdf`, `Testkonzept_Parkhaus_Pico_v1.pdf`), plus the
design of the current implementation. Work from these files; do not re-read the PDFs unless something here is
missing.

## System in one paragraph

School project (M450, BBZW Sursee, class sina24cL; team Noah Bachmann + Paul Willimann; client/teacher
Adrian Elsener). A car park barrier and payment system prototype on **two Raspberry Pi Pico W**
(Arduino framework, earlephilhower core, PlatformIO, Unity tests). **Pico 1 "Kasse"** stores the parking
sessions, calculates the fee, books payments and answers Pico 2 over a JSON API. **Pico 2 "Schranken"** drives
both barrier servos and safety loops, holds the registrations and the event log and serves **all web pages**:
camera simulation (plate typed in), Automat, Tarif, and after login Kasse, Registrierung and Betrieb. On exit,
Pico 2 asks Pico 1 to close the session; the barrier opens only if it is paid or free. Both talk HTTP over a
separate test WLAN. A PC simulator (Docker) runs the same `lib/` code without hardware.

## State

- Everything in the Testkonzept scope that the Pico setup allows is implemented (status per story in
  [requirements.md](requirements.md); deviations from the Testkonzept in [test-concept.md](test-concept.md)).
- Verified with the native unit and system tests and in the PC simulator. **The firmware has not run on the
  Picos** since the user lost access to the hardware; it is only built.
- Open: TF-33 (usability with a stranger) and TF-34 round 2 (exploratory) are manual; timing goals (≤ 2 s,
  ≤ 3 s, < 1 s), servo movement, WLAN and power cuts need the hardware.

## Files

| File | Contents | Read when |
|---|---|---|
| [assignment.md](assignment.md) | Teacher's assignment, deliverables, servo wiring | README / submission work, wiring |
| [requirements.md](requirements.md) | US-01..22 with ACs and status | Implementing or checking a feature |
| [test-concept.md](test-concept.md) | Test goals, strategy, TF-01..34, test data, exact UI strings, deviations | Writing tests, UI text, behaviour details |
| [tariff.md](tariff.md) | **Source of truth** for fee rules + reference values | Anything touching the fee |
| [architecture.md](architecture.md) | Hardware, code layout, app layer, flows, pages, interface, data, simulator | Before writing code |

Human docs (German): `README.md` (submission), `docs/testprotokoll.md` (test protocol and report),
`docs/aufbau-anleitung.md` (wiring, WLAN, flashing).

## Language rules

- Code, identifiers, comments, agent docs (this folder): **English**.
- `README.md` and any documentation for humans: **German**.
- User-visible UI strings: **German**, verbatim as quoted in test-concept.md (tests compare them).
