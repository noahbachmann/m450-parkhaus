# Agent context — index

Condensed, English version of the three source PDFs (kept untracked in the repo root:
`aufgabe-umsetzung-parksystem.pdf`, `Anforderungen_UserStories.pdf`, `Testkonzept_Parkhaus_Pico.pdf`).
Work from these files; do not re-read the PDFs unless something here is missing.

## System in one paragraph

School project (M450, BBZW Sursee, class sina24cL; team Noah Bachmann + Paul Willimann; client/teacher
Adrian Elsener). A car park barrier and payment system prototype on **two Raspberry Pi Pico W**
(Arduino framework, earlephilhower core, PlatformIO, Unity tests). **Pico 1 "Kasse"** serves the
cashier and pay-machine web UIs, calculates the fee, stores parking sessions and answers payment-status
queries. **Pico 2 "Schranken"** drives both barrier servos, reads the safety loops, serves the
registration UI and a web "camera simulation" where the licence plate is typed in. On exit, Pico 2 asks
Pico 1 whether the plate's fee is paid. Both talk HTTP over a separate test WLAN.

## Current phase: phase 1 = assignment minimum + registration

Build only this for now (everything else in the Testkonzept is the later target):

1. Plate registration (Pico 2 web UI, validation, normalization, no duplicates, persistent).
2. Entry: plate typed at entry → registered? → Pico 1 opens a session → entry servo opens.
   Optional backdated entry time. Pico 1 unreachable → reject, barrier stays closed.
3. Parking session with entry timestamp; one open session per plate.
4. Fee calculation per [tariff.md](tariff.md).
5. Payment at Kasse and Automat (simulated: Karte OK, Karte abgelehnt, TWINT, Bargeld with change).
6. Exit: Pico 2 queries Pico 1 → paid or free (CHF 0.00) → exit servo opens, session closed;
   otherwise stays closed (fail-safe, also when Pico 1 is unreachable).

Phase-1 test cases are flagged in [test-concept.md](test-concept.md); user story status in
[requirements.md](requirements.md).

## Files

| File | Contents | Read when |
|---|---|---|
| [assignment.md](assignment.md) | Teacher's assignment, deliverables, servo wiring | README / submission work, wiring |
| [requirements.md](requirements.md) | US-01..22 with ACs and scope status | Implementing or checking a feature |
| [test-concept.md](test-concept.md) | Test goals, strategy, TF-01..34, test data, exact UI strings | Writing tests, UI text, behaviour details |
| [tariff.md](tariff.md) | **Source of truth** for fee rules + reference values | Anything touching the fee |
| [architecture.md](architecture.md) | Pico split, flows, data model, interface, layout, open questions | Before writing code |
| [roadmap.md](roadmap.md) | Implementation steps 1–7 with done criteria and status | Picking the next task |

## Language rules

- Code, identifiers, comments, agent docs (this folder): **English**.
- `README.md` and any documentation for humans: **German**.
- User-visible UI strings: **German**, verbatim as quoted in test-concept.md (tests compare them).
