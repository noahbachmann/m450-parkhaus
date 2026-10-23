# Testprotokoll Phase 1

Stand: 23.10.2026. Testumgebung **TU-1** (PC, `pio test -e native`), weil ab Roadmap-Schritt 4 keine Picos
mehr zur Verfügung standen. Die Systemtests TU-2/TU-3 auf der Hardware konnten deshalb nicht durchgeführt
werden.

## Vorgehen

Die Phase-1-Testfälle aus dem Testkonzept laufen als Systemtest auf dem PC:
`test/native/test_system/system.test.cpp`. Dort arbeitet dieselbe Logik wie auf den Picos zusammen:

- **Pico 2:** Registrierungen, Ein-/Ausfahrtsentscheid (`lib/gate`), Schrankensteuerung (`lib/barrier`)
- **Pico 1:** Schnittstelle `/api/entry`, `/api/exit`, `/api/status` (`lib/kasse_api`), Bezahlen an Kasse
  und Automat (`lib/kasse_view`), Seiten Kasse/Automat
- Statt HTTP ruft Pico 2 die Schnittstelle von Pico 1 direkt im selben Programm auf. «Pico 1 aus» heisst:
  keine Antwort, genau wie bei einem Timeout.
- Die Uhr ist simuliert (Sekunden für die Parkdauer, Millisekunden für die Schranke).
- «Neustart» heisst: Daten ins Dateiformat von LittleFS schreiben und wieder einlesen.

Nur die Glue-Schicht in `src/` (Webserver, HTTPClient, Servo, LittleFS, WLAN) wird nicht getestet; sie
wird mit `pio run` nur gebaut.

## Ergebnisse

Ergebnis = Ausgabe von `pio test -e native`, Suite `test_system` (dazu die Unit-Tests der einzelnen Module).

| TF | Prio | Testfunktion | Geprüft | Ergebnis |
|---|---|---|---|---|
| TF-01 | 1 | `test_tf01_register_survives_reboot` | «Kennzeichen registriert», `NW654321` in der Liste, nach Neustart noch da | bestanden |
| TF-02 | 2 | `test_tf02_invalid_plates_not_stored` | KZ-05/06/07/09 abgelehnt, nichts gespeichert; KZ-13 `UR1` gespeichert | bestanden |
| TF-03 | 2 | `test_tf03_duplicate_plate_rejected` | «Kennzeichen bereits registriert» für beide Schreibweisen, KZ-01 bleibt Kunde A | bestanden |
| TF-04 | 1 | `test_tf04_entry_opens_and_closes_after_loop_released` | Öffnet sofort, bleibt offen solange Schleife belegt, schliesst danach | bestanden (Zeit ≤ 2 s nicht messbar) |
| TF-06 | 2 | `test_tf06_normalized_plate_matches_at_exit` | `lu 123 456` → eine Sitzung `LU123456`, Ausfahrt mit Leerzeichen öffnet | bestanden |
| TF-07 | 1 | `test_tf07_unregistered_stays_closed_no_session` | Bleibt zu, Hinweis Ruftaste, keine Sitzung an der Kasse | bestanden |
| TF-08 | 1 | `test_tf08_ten_entry_cycles` | 10 Zyklen KZ-01/KZ-02 abwechselnd, jedes Mal geöffnet | Ablauf bestanden, Zeitmessung (Video) nicht möglich |
| TF-09 | 1 | `test_tf09_entry_time_and_running_duration` | Einfahrtszeit sekundengenau, Format `TT.MM.JJJJ HH:MM:SS`, Dauer wächst | bestanden |
| TF-10 | 2 | `test_tf10_second_entry_rejected` | «Parkvorgang bereits offen – bitte Ruftaste drücken», eine Sitzung, Zeit unverändert | bestanden |
| TF-11 | 2 | `test_tf11_kasse_off_at_entry` | Ohne Pico 1: zu, «Kasse nicht erreichbar …», keine Sitzung; danach Zeit des 2. Versuchs | bestanden |
| TF-18 | 1 | `test_tf18_reference_fees_via_backdated_entry` | R-01..R-11 über rückdatierte Einfahrt an der Kasse exakt; R-12 abgelehnt | bestanden |
| TF-19 | 2 | `test_tf19_duration_and_fee_at_kasse_and_automat` | «1 h 20 min», «CHF 4.00», Tarifhinweis an Kasse und Automat (Tarifseite: Phase 2) | bestanden |
| TF-20 | 1 | `test_tf20_card_payment_once` | «Bezahlt – CHF 4.00, Karte» + Transaktionsnummer; 2. Versuch «bereits bezahlt» | bestanden |
| TF-21 | 1 | `test_tf21_automat_cash_and_twint` | Rückgeld CHF 4.00; TWINT; «Restbetrag CHF 1.00», dann bezahlt, Rückgeld CHF 0.00 | bestanden |
| TF-22 | 2 | `test_tf22_automat_without_session` | «Kein offener Parkvorgang für SZ555», kein Zahlformular, keine Buchung | bestanden |
| TF-23 | 1 | `test_tf23_paid_exit_closes_session` | Öffnet, «abgeschlossen», Ausfahrtszeit gesetzt, Dauer bleibt stehen | bestanden |
| TF-24 | 1 | `test_tf24_unpaid_exit_then_pay_then_exit` | «Gebühr CHF 6.00 nicht bezahlt – bitte am Automaten bezahlen», nach Zahlung offen | bestanden |
| TF-25 | 2 | `test_tf25_free_time_exit` | Öffnet, CHF 0.00, «abgeschlossen (Freizeit)» | bestanden |
| TF-29 | 1 | `test_tf29_status_interface_twenty_requests` | 20 Abfragen: bezahlt / unbezahlt / unbekannt / HTTP 400 | bestanden (Antwortzeit < 1 s nicht messbar) |
| TF-30 | 1 | `test_tf30_kasse_off_at_exit_paid_status_persisted` | Ohne Pico 1 beide zu; nach Neustart KZ-01 noch bezahlt und öffnet | bestanden |
| TF-32 | 1 | `test_tf32_whole_system` | Registrieren, Einfahrt −2 h 30 min, CHF 6.00, 1. Ausfahrt zu, Rückgeld CHF 4.00, 2. Ausfahrt offen | bestanden |

Gefundene Fehler: keine.

## Nicht geprüft (nur mit Hardware möglich)

| Punkt | Betroffene TF | Grund |
|---|---|---|
| Öffnungszeit ≤ 2.0 s (Eingabe bis Arm oben) | TF-04, TF-08 | braucht Servo und Video |
| Bezahlt-Status innert 3 s, Antwort < 1 s | TF-20, TF-21, TF-29 | Laufzeit auf dem Pico; auf dem PC läuft alles im Mikrosekundenbereich |
| Bewegung der Servos, Taster als Schleife | TF-04 | keine Hardware |
| WLAN, feste IPs, HTTP-Timeout 1000 ms | TF-11, TF-30 | im Test durch «keine Antwort» ersetzt |
| Echter Stromunterbruch während des Schreibens | TF-11, TF-30 | LittleFS-Glue in `src/common/storage.cpp` nur gebaut |
| Uhrzeit per NTP (±2 s) | TF-09, TF-23 | im Test simulierte Uhr |
| Webseiten im Browser bedienen | alle UI-Fälle | HTML wird im Test auf Inhalt geprüft, nicht im Browser |
