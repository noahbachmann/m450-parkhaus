# Tariff — source of truth

**This file is authoritative and editable by the user.** When it changes, the agent updates the tariff
code in `lib/` and its Unity tests in `test/` to match (parameters, formula and reference table).
Keep the reference table consistent with the parameters.

## Parameters

| Name | Value | Meaning |
|---|---|---|
| `FREE_MINUTES` | 15 | Stay ≤ this (inclusive, to the second) costs nothing |
| `UNIT_MINUTES` | 60 | Billing unit; every **started** unit counts |
| `PRICE_PER_UNIT` | CHF 2.00 | Price per started unit |
| `DAY_BLOCK_HOURS` | 24 | Length of one daily-cap block, counted from entry |
| `DAILY_MAX` | CHF 20.00 | Cap per started day block |
| Free minutes scope | whole stay | Once exceeded, billing starts at entry (free minutes are not deducted) and they don't apply again in later day blocks |

German wording (from Testkonzept 11.2.2): «15 Freiminuten; wird die Freizeit überschritten, kostet jede
angefangene Stunde ab Einfahrt CHF 2.00; Tagesmaximum CHF 20.00 pro angefangene 24 Stunden.»

## Rule

```
d = exit_or_now - entry          (seconds)
if d < 0                 -> error "invalid duration", no fee
if d <= FREE_MINUTES*60  -> 0
else
  fee = 0
  for each started DAY_BLOCK in d:
      block_seconds = seconds of d falling into this block
      fee += min(ceil(block_seconds / (UNIT_MINUTES*60)) * PRICE_PER_UNIT, DAILY_MAX)
```

Implementation hints: calculate in integer Rappen (1 CHF = 100), with integer durations in seconds. Pure
function in `lib/`, with time passed in, so it can be tested under `native`.

## Reference values

Dev tests (TU-1) use the exact durations. The system test (TF-18) uses a value 1 min on the safe side of
the boundary, because time keeps running between entry and reading the value.

| ID | Exact duration (unit test) | System-test value | Expected CHF | Boundary |
|---|---|---|---|---|
| R-01 | 0 min 00 s | read immediately | 0.00 | lower bound |
| R-02 | 15 min 00 s | 14 min | 0.00 | last second of free time |
| R-03 | 15 min 01 s | 16 min | 2.00 | first second after free time |
| R-04 | 1 h 00 min 00 s | 59 min | 2.00 | exactly 1 hour |
| R-05 | 1 h 00 min 01 s | 1 h 01 min | 4.00 | started 2nd hour |
| R-06 | 2 h 30 min | 2 h 30 min | 6.00 | normal case |
| R-07 | 10 h 00 min 00 s | 9 h 59 min | 20.00 | last second without cap |
| R-08 | 10 h 00 min 01 s | 10 h 01 min | 20.00 | daily max applies |
| R-09 | 24 h 00 min 00 s | 23 h 59 min | 20.00 | exactly 24 h |
| R-10 | 24 h 00 min 01 s | 24 h 01 min | 22.00 | started 2nd day |
| R-11 | 48 h 00 min 00 s | 47 h 59 min | 40.00 | two full days |
| R-12 | entry time in the future | 1 h in the future | error | invalid duration, no calculation |

Other values used in tests: 1 h 20 min → 4.00 (TF-19), 58 min → 2.00 and > 60 min → 4.00 (TF-27),
5 min → 0.00 (TF-25).
