# Code-Review Parkhaus

Review des Stands `main` (Merge von M-09, `2e886a9`) für den Auftrag «Reviews, Pair Programming und Refactoring». Ziel:
besser lesbarer und wartbarer Code bei **unverändertem Verhalten**.

## Vorgehen

- Gemeinsames Review in der Gruppe (Noah Bachmann, Paul Willimann). Wir sind Datei für Datei durch `lib/`, `src/` und
  `include/` gegangen, wer den Code geschrieben hat, hat ihn erklärt. Die Tests (`test/native/`) haben wir nur als
  Sicherheitsnetz angeschaut, nicht reviewt.
- Checkliste aus dem Auftrag: Namen, Magic Numbers, lange Funktionen, duplizierter Code, tiefe Verschachtelung, erklärende
  Kommentare, toter Code, Vermischung von Logik und Hardware.
- Priorität: **hoch** = schadet der Verständlichkeit an zentraler Stelle oder Duplikat, das auseinanderlaufen kann;
  **mittel** = lokal störend; **tief** = Kosmetik.
- Zeilennummern beziehen sich auf `main` vor dem Refactoring.

## Findings

| Nr | Datei:Zeile | Kategorie | Problem | Vorschlag | Prio | Status |
|---|---|---|---|---|---|---|
| F-01 | `lib/gate/gate.cpp:72-133`, `140-197` | Lange Funktion, Duplikat | `entryRequest` (62 Zeilen) und `exitRequest` (58) wiederholen denselben Ablauf: Kennzeichen prüfen → JSON bauen → POST → «nicht erreichbar» → Antwort mit `if`-Ketten über Status und Fehlercode abbilden. `exitRequest` parst den Fehlercode nochmals selbst (Z. 171), obwohl es `errorCode()` gibt. | Hilfsfunktionen `postPlate()`, `confirmsPlate()`, `unpaidResult()` und eine Tabelle `{Status, Code, Text}` für die Fehlertexte. | hoch | |
| F-02 | `lib/terminal/terminal.cpp:162`, `184` | Duplikat | «Kasse nicht erreichbar – bitte Ruftaste drücken» und «Unerwartete Antwort der Kasse – …» stehen hier nochmals als Literal, in `gate.cpp:11-12` als Konstante. Ein Tippfehler in nur einer Kopie fällt erst im Systemtest auf. | Texte bzw. `unreachableResult()` aus `gate.h` verwenden. | hoch | |
| F-03 | `lib/barrier/barrier.cpp:37-92` | Lange Funktion, Verschachtelung | `Barrier::update` (56 Zeilen) macht zwei Dinge: Schleife/Störung verfolgen und den Zustandsautomaten schalten. Im `switch` stecken `if/else` auf 3 Ebenen, ein `return` mitten im `switch` (Z. 75) und eine unbenannte Winkelformel (Z. 85-87). | Aufteilen in `trackLoop()`, `stepStateMachine()`, `sweepAngle()`. | hoch | |
| F-04 | `gate.cpp`, `terminal.cpp`, `kasse_api.cpp`, `kasse_client.cpp`, `operator_view.cpp` | Magic Numbers | HTTP-Statuscodes als nackte Zahlen (`201`, `402`, `409`, `503` …), über 60 Stellen. Wer `402` liest, muss wissen, dass das «Payment Required» ist. | Benannte Konstanten `HTTP_OK`, `HTTP_CREATED`, `HTTP_PAYMENT_REQUIRED` … in `lib/web`. | hoch | |
| F-05 | `lib/schranken_view/schranken_view.cpp:62-84` | Duplikat | Dasselbe Formular «POST + verstecktes Feld `lane` + Knopf» ist dreimal ausgeschrieben (Durchfahren, Schleife, Ruftaste), Z. 72-75 nochmals doppelt im Ternary. | Hilfsfunktion `appendLaneAction(html, action, lane, label, …)`. | mittel | |
| F-06 | `lib/auth/auth.cpp:36-48`, `lib/schranken_app/schranken_app.cpp:458-466` | Duplikat, Magic Numbers | Dieselbe Hex-Schleife (`HEX_DIGITS`, `shift = 28`, `0xF`) zweimal; `4` Wörter pro Token als Zahl (`auth.cpp:40`, `schranken_app.cpp:291`). | `appendHex()` in `lib/auth`, Konstante `TOKEN_WORDS`. | mittel | |
| F-07 | `lib/schranken_app/schranken_app.cpp:50-63`, `490-494` | Magic Strings | Dateinamen `"registrations"` und `"events"` je dreimal als Literal; ein Tippfehler hiesse: Daten werden in eine andere Datei geschrieben als gelesen. | `REGISTRATIONS_FILE`, `EVENTS_FILE`. | mittel | |
| F-08 | `lib/schranken_app/schranken_app.cpp:159-307` | Magic Strings | `redirect("/kamera")` 12× ausgeschrieben. | Konstante `CAMERA_PAGE`. | tief | |
| F-09 | `lib/schranken_app/schranken_app.cpp:414`, `440` | Duplikat | `l.barrier.loopFault() ? loopFaultMessage(gateLaneLabel(l.id)) : ""` zweimal. | `faultText(Lane&)`. | tief | |
| F-10 | `lib/schranken_app/schranken_app.h:104-105`, `.cpp:215-243` | Namen | Der Handler heisst `loop` (Verwechslung mit Arduino `loop()`), der Handler `driveThrough` verdeckt die gleichnamige freie Funktion, darum `::driveThrough` (Z. 219). | `setLoop`, `passVehicle`. | mittel | |
| F-11 | `lib/schranken_app/schranken_app.cpp:389-399` | Namen | `show()` zeigt nicht nur an, sondern öffnet die Schranke, schreibt ins Ereignisprotokoll und auf die Konsole. Der Name versteckt die wichtigste Nebenwirkung. | `applyGateResult()`. | mittel | |
| F-12 | `src/schranken/lane.h:58` | Namen | Der Debouncer der Sicherungsschleife heisst `loop`, in einer Datei mit `loop1()` und `loopPin`. | `loopDebouncer`. | tief | |
| F-13 | `src/hwtest/main.cpp:25-26`, `32` | Magic Numbers | `write(open ? 90 : 0)` und `delay(2000)`, obwohl `BARRIER_OPEN_ANGLE`/`BARRIER_CLOSED_ANGLE` in `barrier.h` existieren. Der Kommentar Z. 6 wiederholt die Zahlen. | Konstanten verwenden, `TOGGLE_INTERVAL_MS`. | mittel | |
| F-14 | `lib/kasse_api/kasse_api.cpp:66-225` | Duplikat | `handleEntry`, `handleExit`, `handlePay` prüfen je: Body parsen → `bad_request` → `invalid_plate` → `time_not_synced`. | Gemeinsame Vorprüfung `checkMutation()`. | mittel | |
| F-15 | `lib/kasse_api/kasse_api.cpp`, `lib/kasse_client/kasse_client.cpp:54-85` | Duplikat | Die Zuordnung Fehlerfall ↔ (Status, Code) ist auf der Serverseite geschrieben und im Client als 7-fache `else if`-Kette nochmals umgekehrt. | Eine gemeinsame Tabelle für beide Seiten. | mittel | |
| F-16 | `lib/kasse_api/kasse_api.cpp:52-205`, `lib/operator_view/operator_view.cpp:47-57` | Namen | Die API mischt deutsche (`"bezahlt"`, `"unbekannt"`) und englische Statuswörter (`"closed_free"`). | Ein Vokabular. | mittel | |
| F-17 | `lib/gate/gate.h:30`, `lib/html`, `lib/schranken_view` | Namen, Typen | CSS-Klasse `cls` als `const char*` (`"ok"`/`"err"`/`""`), «hat Notiz» wird über `cls[0] != '\0'` geprüft. | `enum class Tone`, `Notice::empty()`. | mittel | |
| F-18 | `lib/kasse_view/kasse_pages.cpp:152-234` | Lange Funktion | `renderPayPage` 83 Zeilen: Einleitung, Fehler, Suchformular, Sitzung, Zahlformular, Liste. | In `appendSearchForm`, `appendSessionLookup` … aufteilen. | mittel | |
| F-19 | `lib/operator_view/operator_view.cpp:207-280` | Lange Funktion | `remoteOpen` 74 Zeilen: Validierung, Registrierung, HTTP-Aufruf, Protokoll. | Aufteilen. | mittel | |
| F-20 | `lib/operator_view/operator_view.cpp:174-205` | Vermischung | `laneStatus()` in einem View-Modul macht den HTTP-Aufruf und parst JSON. | Abfrage in `kasse_client`, View erhält nur Daten. | mittel | |
| F-21 | `lib/kasse_client/kasse_client.cpp:16-88` | Lange Funktion | `paymentFromReply` 73 Zeilen. | Zusammen mit F-15 tabellengesteuert. | tief | |
| F-22 | `lib/session/payment.h:14`, `lib/session/session.h:22` | Namen | Zwei Enums `PayMethod` und `PaymentMethod` mit ähnlicher Bedeutung, verschieden geparst. | Umbenennen nach Rolle (`PayRequestMethod`). | mittel | |
| F-23 | `src/schranken/main.cpp:38-129` | Vermischung | `HttpKasseLink` und `BoardPlatform` (90 Zeilen Adaptercode) stehen in `main.cpp`. | Eigene Dateien in `src/schranken/`. | tief | |
| F-24 | `src/common/net.cpp:70-131` | Lange Funktion | `netUpdate` 62 Zeilen: Status loggen, Verbindung, Reconnect, NTP. | Aufteilen. | mittel | |
| F-25 | `src/common/net.cpp:44-48` | Magic Numbers | NTP-Fallback-IP und Hostnamen inline. | Konstanten. | tief | |
| F-26 | `lib/format/format.cpp:99-165`, `lib/kasse_view/kasse_pages.cpp:103` | Magic Numbers | `3600`, `7200`, `60` für Stunden/Zeitzone mehrfach; `kasse_pages` hat eigene `MIN`/`HOUR`. | `SECONDS_PER_HOUR`, `CET_OFFSET` … | tief | |
| F-27 | `src/sim/common/http_server.cpp:51-138`, `http_client.cpp:79-175` | Lange Funktion | `readRequest` 88 und `httpRequest` 97 Zeilen (nur Simulator). | Aufteilen. | tief | |
| F-28 | `src/sim/common/http_client.cpp:119`, `http_server.cpp:110` | Duplikat | Header `X-Api-Key` nochmals als Literal statt `API_KEY_HEADER`. | Konstante verwenden. | tief | |
| F-29 | `lib/codec/codec.cpp:126` | Kommentar erklärt Umweg | «add() again instead of assigning, so list.revision() moves on» – fehlende Methode wird umschifft. | `RegistrationList::replaceAll()`. | tief | |
| F-30 | `lib/auth/auth.cpp:12` | Sicherheit | Passwortvergleich nicht in konstanter Zeit (API-Schlüssel schon, `kasse_api.cpp:245`). | `apiKeyMatches` nach `auth` verschieben und für beide verwenden. | tief | |

### Fehler, die beim Review gefunden wurden

| Nr | Datei:Zeile | Problem | Vorgehen |
|---|---|---|---|
| B-01 | `src/sim/common/http_server.cpp:13-36` | `statusText()` kennt 403 nicht, der Simulator sendet darum die Statuszeile `HTTP/1.0 403 Status`. Browser und Pico 2 werten nur die Zahl aus, also kein sichtbarer Fehler. | Nicht im Refactoring, weil es das Verhalten ändert; separater `fix:` auf `main`. |

### Bereits gut, bewusst nicht angefasst

- **Trennung Logik / Hardware:** Gebühr (`lib/tariff`), Schranke (`lib/barrier`), Routen (`lib/schranken_app`, `routeKasse`)
  sind ohne `Arduino.h`, Zeit kommt als Parameter. Hardware nur in `src/` hinter `BarrierPort`, `KasseLink`,
  `AppPlatform`. Der Auftrag nennt «Gebührenberechnung mit Servo/Serial vermischt» – das gibt es bei uns nicht.
- **Konstanten:** Pins nur in `include/pins.h`, Tarif in `tariff.h`, Schrankenzeiten und Winkel in `barrier.h` – jeweils
  mit Begründung im Kommentar.
- **Kein grosses `loop()`:** `setup()`/`loop()` von Pico 2 sind 13 bzw. 27 Zeilen; die Routen sind eine Tabelle (`ROUTES`)
  statt einer `if`-Kette.
- **Kein toter oder auskommentierter Code**, keine `TODO`/`#if 0`. `onBarrierEvent` sieht überflüssig aus, wird aber
  vom Simulator und den Tests gebraucht.
- **Ein- und Ausfahrt** teilen sich bereits eine Klasse (`Barrier`, `CoreLane`, `LaneTerminal`); dupliziert sind nur
  die Kassenabfragen (F-01) und die Formulare (F-05).
