# Testprotokoll und Testbericht

Nach Testkonzept «Parkhaus-Schranken (Raspberry Pi Pico)» v1.0, Kapitel 6.2 (Mängelklassen), 6.3
(Endkriterien) und 11.3 (Testprotokoll, Testbericht).

| | |
|---|---|
| Datum | 24.10.2026 |
| Tester | N. Bachmann (automatisierte Tests mit `pio test -e native`; TF-34 Runde 1 von Hand im PC-Simulator) |
| Software-Version | Commit `a09a3a2` (Branch `main`) |
| Testumgebung | **TU-1** (PC). TU-2/TU-3 (Aufbau mit den Picos) nicht möglich: Die Hardware stand nicht mehr zur Verfügung. Zusätzlich von Hand im PC-Simulator (Docker). |
| Testdaten | Kennzeichen KZ-01 bis KZ-13, Tarif R-01 bis R-12, Zahlungsmittel ZM-01 bis ZM-04 gemäss Testkonzept 11.2 (`lib/testdata`, `lib/test_support/test_fixtures.h`) |

## Vorgehen

Die Systemtests laufen auf dem PC mit derselben Logik wie auf den Picos (`test/native/test_system_*`):

- **Pico 2** ist die echte Anwendung (`lib/schranken_app`). Der Test bedient sie über dieselben Seiten und
  Formulare wie der Browser: Kamerasimulation, Automat, Kasse, Registrierung, Leitstelle.
- **Pico 1** antwortet über dieselbe Schnittstelle wie auf dem Board (`routeKasse`). «Pico 1 aus» heisst: keine
  Antwort, wie bei einem Timeout.
- **Sicherungsschleife:** «Fahrzeug bleibt stehen» / «Fahrzeug fährt weiter» auf der Kamerasimulation.
- **Uhr:** simuliert (Sekunden für die Parkdauer, 100-ms-Schritte für die Schranken). «Neustart» heisst: der Pico
  wird aus seinen gespeicherten Dateien neu aufgebaut.
- Nicht getestet wird nur die Anbindung an die Hardware in `src/` (Webserver, HTTPClient, Servo, LittleFS, WLAN);
  sie wird mit `pio run` gebaut.

**Ergebnis:** «bestanden» = alle Erwartungen des Testfalls geprüft und erfüllt. «teilweise» = Ablauf bestanden,
aber eine Erwartung ist nur auf der Hardware prüfbar (meist eine Zeitmessung). Die Mängel stehen unten in der
Mängelliste (statt als Git-Issues).

## Ergebnisse

### Registrierung, Einfahrt, Parkvorgang (`test_system_entry`)

| TF | Prio | Ergebnis | Nachweis | Bemerkung | Mängel-ID |
|---|---|---|---|---|---|
| TF-01 | 1 | bestanden | `test_tf01_register_survives_reboot` | «Kennzeichen registriert: NW654321», nach Neustart in der Liste | – |
| TF-02 | 2 | bestanden | `test_tf02_invalid_plates_not_stored` | KZ-05/06/07/09 mit Fehlermeldung abgewiesen, KZ-13 gespeichert | – |
| TF-03 | 2 | bestanden | `test_tf03_duplicate_plate_rejected` | beide Schreibweisen «Kennzeichen bereits registriert», KZ-01 bleibt Kunde A | – |
| TF-04 | 1 | teilweise | `test_tf04_entry_opens_and_closes_after_the_car_passed` | öffnet in derselben Anfrage, schliesst erst nach Freigabe der Schleife; **Öffnungszeit ≤ 2 s nicht messbar** | – |
| TF-05 | 2 | bestanden | `test_tf05_terminal_feedback_at_entry` | «LU123456 – Willkommen, Einfahrt frei», «BE4711 – nicht registriert, bitte Ruftaste drücken» | – |
| TF-06 | 2 | bestanden | `test_tf06_normalized_plate_matches_at_exit` | `lu 123 456` → ein Parkvorgang `LU123456`, Ausfahrt mit Leerzeichen öffnet | – |
| TF-07 | 1 | bestanden | `test_tf07_unregistered_stays_closed_no_session` | bleibt zu, kein Parkvorgang | – |
| TF-08 | 1 | teilweise | `test_tf08_ten_entry_cycles` | 10 Zyklen geöffnet; **Zeitmessung per Video nicht möglich** | – |
| TF-09 | 1 | teilweise | `test_tf09_entry_time_and_running_duration` | Einfahrtszeit `TT.MM.JJJJ HH:MM:SS`, Dauer wächst; **Abgleich ±2 s mit Referenzuhr (NTP) nur auf Hardware** | – |
| TF-10 | 2 | bestanden | `test_tf10_second_entry_rejected` | «Parkvorgang bereits offen – bitte Ruftaste drücken» bei Abstand ≥ 30 s (Abweichung, siehe README) | M-05 |
| TF-11 | 2 | bestanden | `test_tf11_kasse_off_at_entry` | ohne Pico 1 zu, «Kasse nicht erreichbar – bitte Ruftaste drücken», kein Parkvorgang; danach Zeit des 2. Versuchs | – |

### Schrankensicherheit, Fernöffnung (`test_system_safety`)

| TF | Prio | Ergebnis | Nachweis | Bemerkung | Mängel-ID |
|---|---|---|---|---|---|
| TF-12 | 1 | bestanden | `test_tf12_stays_open_while_loop_occupied` | an beiden Schranken 30 s belegt: bleibt offen, schliesst nach Freigabe | – |
| TF-13 | 1 | teilweise | `test_tf13_tf14_stop_while_closing_and_six_log_entries` | 6 Durchgänge: Stopp und ganz hoch im nächsten Durchlauf; **< 0.5 s und Servobewegung nur auf Hardware** | M-08 |
| TF-14 | 2 | bestanden | `test_tf13_tf14_stop_while_closing_and_six_log_entries` | genau 6 Einträge «Sicherheitsabschaltung» mit Schranke und Zeit, Filter in der Leitstelle | – |
| TF-15 | 2 | bestanden | `test_tf15_loop_fault_after_five_minutes` | 4:50 keine Meldung, 5:10 «Sicherungsschleife Einfahrt länger als 5 min belegt» auf der Seite und im Protokoll (±2 s), Schranke bleibt offen | – |
| TF-16 | 2 | bestanden | `test_tf16_remote_opening_at_entry` | Leitstelle zeigt «Einfahrt», «BE4711», «kein Parkvorgang», Ruf; öffnet, Parkvorgang eröffnet, Protokoll mit Benutzer, Zeit, Spur, Grund | – |
| TF-17 | 2 | bestanden | `test_tf17_remote_opening_needs_reason` | «Bitte Grund auswählen», bleibt zu, nichts protokolliert | – |

Zusätzlich: `test_remote_opening_with_kasse_off_stays_closed` (Fernöffnung ohne Kasse bleibt zu, protokolliert),
`test_new_customer_by_operator_then_automat_and_exit` (M-03), `test_drive_through_closes_and_reports` (M-02),
`test_call_button_reaches_operator`.

### Gebühr, Bezahlung, Ausfahrt, Gesamtsystem (`test_system_payment`)

| TF | Prio | Ergebnis | Nachweis | Bemerkung | Mängel-ID |
|---|---|---|---|---|---|
| TF-18 | 1 | bestanden | `test_tf18_reference_fees_to_the_second` | R-01 bis R-11 sekundengenau über die rückdatierte Einfahrt am Automaten, R-12 abgewiesen | – |
| TF-19 | 2 | bestanden | `test_tf19_duration_fee_and_tariff_at_kasse_and_automat` | Kasse und Automat: «1 h 20 min», «CHF 4.00», 2 angefangene Stunden + 15 Freiminuten, Tarif verlinkt und gleich wie tariff.md | M-07 |
| TF-20 | 1 | teilweise | `test_tf20_payment_at_kasse_once` | «Bezahlt – CHF 4.00, Karte – Transaktion TX-000001», 2. Versuch «bereits bezahlt»; **≤ 3 s nur auf Hardware** | – |
| TF-21 | 1 | teilweise | `test_tf21_automat_cash_change_and_twint` | Rückgeld CHF 4.00, TWINT, «Restbetrag CHF 1.00 – unbezahlt», dann Rückgeld CHF 0.00; **≤ 3 s nur auf Hardware** | M-06, M-11 |
| TF-22 | 2 | bestanden | `test_tf22_automat_without_session` | «Kein offener Parkvorgang für SZ555», kein Zahlformular, keine Buchung | M-11 |
| TF-23 | 1 | teilweise | `test_tf23_paid_exit_closes_session` | öffnet, «abgeschlossen», Ausfahrtszeit gesetzt, Dauer bleibt stehen; **≤ 2 s und ±2 s zur Referenzuhr nur auf Hardware** | – |
| TF-24 | 1 | bestanden | `test_tf24_unpaid_exit_then_pay_then_exit` | «Gebühr CHF 6.00 nicht bezahlt – bitte am Automaten bezahlen», nach Zahlung offen | – |
| TF-25 | 2 | bestanden | `test_tf25_free_time_exit` | öffnet, CHF 0.00, «abgeschlossen (Freizeit)» | – |
| TF-26 | 2 | bestanden | `test_tf26_exit_without_session` | «Kein Parkvorgang gefunden – bitte Ruftaste drücken», zu | – |
| TF-27 | 2 | bestanden | `test_tf27_car_turns_around` | «Fahrzeug wendet»: bleibt «offen», keine Ausfahrtszeit, CHF 2.00 → CHF 4.00 nach 60 min | – |
| TF-28 | 2 | bestanden | `test_tf28_declined_payment_at_exit_terminal` | «Zahlung abgelehnt – anderes Zahlungsmittel wählen», nach TWINT offen; nach 5 Ablehnungen «Leitstelle wurde benachrichtigt» + Eintrag in der Leitstelle | M-09 |
| TF-29 | 1 | teilweise | `test_tf29_status_interface_for_all_states` | 20 Anfragen: bezahlt / unbezahlt / unbekannt / HTTP 400; **Antwortzeit < 1 s nur auf Hardware** | M-12 |
| TF-30 | 1 | bestanden | `test_tf30_kasse_off_at_exit_logged_and_paid_status_kept` | ohne Pico 1 beide zu, Störung mit Spur und Kennzeichen protokolliert; nach Neustart KZ-01 bezahlt, öffnet | – |
| TF-31 | 2 | bestanden | `test_tf31_remote_opening_at_exit_without_payment` | Protokoll mit Zeit, «betrieb», «Ausfahrt», Kennzeichen, «Kulanz», «ohne Zahlung»; «ohne Zahlung ausgefahren» | – |
| TF-32 | 1 | bestanden | `test_tf32_whole_system` | Registrieren, Einfahrt −2 h 30 min, CHF 6.00, 1. Ausfahrt zu, Automat Rückgeld CHF 4.00, 2. Ausfahrt offen | – |

### Manuelle Testfälle

| TF | Prio | Ergebnis | Bemerkung | Mängel-ID |
|---|---|---|---|---|
| TF-33 | 3 | nicht durchgeführt | Usability mit einer fremden Person («Bezahle das Parken für das Auto ZH 98765.»), mit dem PC-Simulator am Automaten möglich | – |
| TF-34 | 3 | teilweise | Runde 1 (23.10.2026, PC-Simulator, Dauer nicht protokolliert) mit den Mängeln M-01 bis M-04; Runde 2 nach den Änderungen vom 24.10.2026 offen | M-01–M-04 |

## Mängelliste

Klassen nach Testkonzept 6.2: 1 belanglos, 2 leicht, 3 schwer, 4 kritisch.

| ID | Klasse | TF / Quelle | Beschreibung | Status |
|---|---|---|---|---|
| M-01 | 2 | TF-34 R1 | Oberfläche unübersichtlich: zwei Navigationen, Admin-Seiten für alle sichtbar, keine Anzeige, ob man angemeldet ist | behoben 23.10.2026: eine Oberfläche auf Pico 2, Anmeldeformular, 🔒/👤-Anzeige |
| M-02 | 2 | TF-34 R1 | Keine Rückmeldung, dass ein Fahrzeug durchgefahren ist | behoben 23.10.2026: «Durchfahren», Meldungen «… – Schranke schliesst / geschlossen» |
| M-03 | 3 | TF-34 R1 | Leitstelle kann bei der Fernöffnung keinen neuen Kunden registrieren | behoben 23.10.2026: Grund «Neuer Kunde registrieren» |
| M-04 | 3 | TF-34 R1 | Beim Testen fehlen auf der Kamerasimulation die aktuellen Kennzeichen | behoben 23.10.2026: Liste der Parkvorgänge und Kennzeichen-Vorschläge |
| M-05 | 2 | Simulator | Antwortet Pico 1 erst nach dem Timeout, ist der Parkvorgang eröffnet, die Schranke aber zu; der 2. Versuch meldete «Parkvorgang bereits offen» | behoben 23.10.2026: Wiederholung innert 30 s gilt als dieselbe Einfahrt |
| M-06 | 3 | Review | Karte/TWINT nach einer Bargeld-Teilzahlung belastete die ganze Gebühr, das eingeworfene Bargeld ging verloren | behoben 24.10.2026: nur der Rest wird belastet, Anzeige «… Karte CHF 1.50 + Bargeld CHF 0.50» |
| M-07 | 2 | Review | Tarifhinweis bei bezahlten Parkvorgängen mit der aktuellen statt der Zahlungszeit; über 24 h zählte er Stunden über den ganzen Aufenthalt | behoben 24.10.2026: Aufschlüsselung aus `lib/tariff`, bei bezahlten Parkvorgängen kein laufender Hinweis |
| M-08 | 4 | Review | Beim Start fuhr die Schranke ohne Prüfung der Schleife zu (Auto unter dem Arm nach einem Neustart) | behoben 24.10.2026: belegte Schleife beim Start → Schranke bleibt offen; auf Hardware ungeprüft |
| M-09 | 2 | Review | Bei vollem Ereignisprotokoll (100 Einträge) wurde ein Zahlungsalarm nicht gespeichert | behoben 24.10.2026: Speichern nach Änderungszähler |
| M-10 | 2 | Review | Unbekanntes Zahlungsmittel in einer Antwort der Kasse wurde als «Karte» angezeigt | behoben 24.10.2026: wird als unerwartete Antwort abgewiesen |
| M-11 | 3 | Review | Automat fehlte (Testobjekt 07, US-17, TF-19/21/22/33): Bezahlen nur an der Kasse mit Anmeldung oder am Ausfahrtsterminal ohne Bargeld | behoben 24.10.2026: öffentliche Seite «Automat» |
| M-12 | 4 | Review | Pico 1 nahm Zahlungen, Ausfahrten und Reset von jedem Gerät im WLAN an (Ausfahrt ohne Zahlung möglich) | behoben 24.10.2026: POST nur von Pico 2; auf Hardware ungeprüft |
| M-13 | 2 | Review | «Durchfahren» entschied auf dem Board nach einem möglicherweise veralteten Schrankenzustand, im Simulator nach dem echten (Board und Simulator wichen ab) | behoben 24.10.2026: eine gemeinsame Anwendung für Board und Simulator, Antwort vom Schrankenkern |
| M-14 | 2 | Review | Ein prellender Taster bzw. verrauschter IR-Sensor konnte Sicherheitsabschaltungen oder eine vorzeitige «Durchfahrt» auslösen | behoben 24.10.2026: Schleife muss 50 ms stabil sein; auf Hardware ungeprüft |

## Nicht geprüft (nur mit Hardware möglich)

| Punkt | Betroffene TF | Grund |
|---|---|---|
| Öffnungszeit ≤ 2.0 s (Eingabe bis Arm oben) | TF-04, TF-08, TF-23 | braucht Servo und Video |
| Bezahlt-Status innert 3 s, Antwort < 1 s | TF-20, TF-21, TF-29 | Laufzeit auf dem Pico; auf dem PC im Mikrosekundenbereich |
| Stopp < 0.5 s, Servo fährt wirklich hoch | TF-13 | Logik reagiert im nächsten Durchlauf auf dem zweiten Prozessorkern, die echte Zeit ist ungemessen |
| Uhrzeit per NTP (±2 s) | TF-09, TF-23 | im Test simulierte Uhr |
| Servos, Taster als Schleife, Start mit belegter Schleife | TF-04, TF-12, TF-13 | keine Hardware |
| WLAN, feste IPs, HTTP-Timeout 1000 ms, Zugriff nur von Pico 2 | TF-11, TF-29, TF-30 | im Test durch «keine Antwort» ersetzt |
| Echter Stromunterbruch während des Schreibens | TF-11, TF-30 | LittleFS-Anbindung in `src/common/storage.cpp` nur gebaut |
| Bedienung im Browser | alle UI-Fälle | HTML wird im Test auf Inhalt geprüft; von Hand im PC-Simulator |

## Testbericht

| | Anzahl |
|---|---|
| Testfälle | 34 |
| bestanden | 24 |
| teilweise (Zeitmessung oder NTP nur auf Hardware) | 9 (TF-04, 08, 09, 13, 20, 21, 23, 29; TF-34 nur Runde 1) |
| nicht durchgeführt | 1 (TF-33) |
| Priorität 1 | 15, davon 7 bestanden, 8 teilweise |
| offene Mängel Klasse 3 oder 4 | 0 (M-08, M-12 und M-14 behoben, aber auf Hardware ungeprüft) |
| Smoke-Test (TF-01, 04, 20, 23, 24) | läuft bei jedem `pio test -e native` mit |

**Endkriterien (6.3):** Alle Testfälle der Priorität 1 und 2 sind durchgeführt, und es sind keine Mängel der
Klasse 3 oder 4 offen. Nicht erfüllt ist «alle Priorität-1-Testfälle bestanden»: Bei 8 Fällen fehlt die Messung
auf der Hardware.

**Empfehlung:** Funktional abnahmebereit. Vor der Abnahme (TU-3) auf dem Aufbau mindestens den Hardware-Schnelltest
(`pio run -e hwtest`), TF-32, die Zeitmessungen TF-08 und TF-13 sowie TF-11/TF-30 mit echtem Stromunterbruch
durchführen und TF-33 und TF-34 Runde 2 im Simulator nachholen.
