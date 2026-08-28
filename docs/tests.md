# Testfälle

Zwei Suiten:

    pio test -e native   Logiktests, nur Host     test/test_countdown/test_countdown.cpp
    pio test -e pico     LED-Smoketest, Board     test/test_hardware/test_led.cpp

## Logiktests (native)

Eine Testfunktion pro Aspekt einer User Story.

| Test | Story | Prüft |
| --- | --- | --- |
| `test_set_duration` | US1 | `setDuration` setzt Dauer und Restzeit, Phase bleibt `IDLE`; Werte ausserhalb 1–3600 s (0, 99999) lassen den Zustand unverändert |
| `test_parse_seconds` | US1 | `parseSeconds` liefert 30 für `"set 30"` und -1 für nicht-numerische (`"set abc"`) sowie leere Eingabe |
| `test_led_shows_the_phase` | US2 | LED aus in `IDLE`, an in `RUNNING`; `start` wechselt nach `RUNNING` mit voller Restzeit |
| `test_reset` | US3 | nach einem Tick von 12 s bleiben 18 s; `reset` geht zurück auf `IDLE`, stellt Restzeit und Dauer wieder her, LED aus |
| `test_countdown_expires` | US4 | `tick` zählt nur in `RUNNING` herunter, wechselt bei null nach `FINISHED`, begrenzt die Restzeit bei Überschuss auf 0 und tut in `IDLE` nichts |
| `test_led_blinks_when_finished` | US4 | in `FINISHED` wechselt die LED alle 500 ms (an bei 0 ms, aus bei 500 ms, an bei 1000 ms) und blinkt auch nach 60 s weiter |

## Hardwaretest (pico)

| Test | Prüft |
| --- | --- |
| `test_led_blinks` | blinkt `LED_BUILTIN` dreimal im 500-ms-Takt — Sichtprüfung, dass Board und LED-Pin funktionieren |
