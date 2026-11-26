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
- Zeilennummern beziehen sich auf `main` vor dem Refactoring. Status: Commit auf dem Branch `refactoring`, die Zahl in
  Klammern verweist auf die Begründung unter [Nicht umgesetzt](#nicht-umgesetzt).

## Findings

| Nr | Datei:Zeile | Kategorie | Problem | Vorschlag | Prio | Status |
|---|---|---|---|---|---|---|
| F-01 | `lib/gate/gate.cpp:72-133`, `140-197` | Lange Funktion, Duplikat | `entryRequest` (62 Zeilen) und `exitRequest` (58) wiederholen denselben Ablauf: Kennzeichen prüfen → JSON bauen → POST → «nicht erreichbar» → Antwort mit `if`-Ketten über Status und Fehlercode abbilden. `exitRequest` parst den Fehlercode nochmals selbst (Z. 171), obwohl es `errorCode()` gibt. | Hilfsfunktionen `postPlate()`, `confirmsPlate()`, `unpaidResult()` und eine Tabelle `{Status, Code, Text}` für die Fehlertexte. | hoch || umgesetzt `be6209c` |
| F-02 | `lib/terminal/terminal.cpp:162`, `184` | Duplikat | «Kasse nicht erreichbar – bitte Ruftaste drücken» und «Unerwartete Antwort der Kasse – …» stehen hier nochmals als Literal, in `gate.cpp:11-12` als Konstante. Ein Tippfehler in nur einer Kopie fällt erst im Systemtest auf. | Texte bzw. `unreachableResult()` aus `gate.h` verwenden. | hoch || umgesetzt `be6209c` |
| F-03 | `lib/barrier/barrier.cpp:37-92` | Lange Funktion, Verschachtelung | `Barrier::update` (56 Zeilen) macht zwei Dinge: Schleife/Störung verfolgen und den Zustandsautomaten schalten. Im `switch` stecken `if/else` auf 3 Ebenen, ein `return` mitten im `switch` (Z. 75) und eine unbenannte Winkelformel (Z. 85-87). | Aufteilen in `trackLoop()`, `stepStateMachine()`, `sweepAngle()`. | hoch || umgesetzt `d998f8c` |
| F-04 | `gate.cpp`, `terminal.cpp`, `kasse_api.cpp`, `kasse_client.cpp`, `operator_view.cpp` | Magic Numbers | HTTP-Statuscodes als nackte Zahlen (`201`, `402`, `409`, `503` …), über 60 Stellen. Wer `402` liest, muss wissen, dass das «Payment Required» ist. | Benannte Konstanten `HTTP_OK`, `HTTP_CREATED`, `HTTP_PAYMENT_REQUIRED` … in `lib/web`. | hoch || umgesetzt `c6e8b76` |
| F-05 | `lib/schranken_view/schranken_view.cpp:62-84` | Duplikat | Dasselbe Formular «POST + verstecktes Feld `lane` + Knopf» ist dreimal ausgeschrieben (Durchfahren, Schleife, Ruftaste), Z. 72-75 nochmals doppelt im Ternary. | Hilfsfunktion `appendLaneAction(html, action, lane, label, …)`. | mittel || umgesetzt `a0ff9a3` |
| F-06 | `lib/auth/auth.cpp:36-48`, `lib/schranken_app/schranken_app.cpp:458-466` | Duplikat, Magic Numbers | Dieselbe Hex-Schleife (`HEX_DIGITS`, `shift = 28`, `0xF`) zweimal; `4` Wörter pro Token als Zahl (`auth.cpp:40`, `schranken_app.cpp:291`). | `appendHex()` in `lib/auth`, Konstante `TOKEN_WORDS`. | mittel || umgesetzt `68b9582` |
| F-07 | `lib/schranken_app/schranken_app.cpp:50-63`, `490-494` | Magic Strings | Dateinamen `"registrations"` und `"events"` je dreimal als Literal; ein Tippfehler hiesse: Daten werden in eine andere Datei geschrieben als gelesen. | `REGISTRATIONS_FILE`, `EVENTS_FILE`. | mittel || umgesetzt `87bf864` |
| F-08 | `lib/schranken_app/schranken_app.cpp:159-307` | Magic Strings | `redirect("/kamera")` 12× ausgeschrieben. | Konstante `CAMERA_PAGE`. | tief || umgesetzt `87bf864` |
| F-09 | `lib/schranken_app/schranken_app.cpp:414`, `440` | Duplikat | `l.barrier.loopFault() ? loopFaultMessage(gateLaneLabel(l.id)) : ""` zweimal. | `faultText(Lane&)`. | tief || umgesetzt `87bf864` |
| F-10 | `lib/schranken_app/schranken_app.h:104-105`, `.cpp:215-243` | Namen | Der Handler heisst `loop` (Verwechslung mit Arduino `loop()`), der Handler `driveThrough` verdeckt die gleichnamige freie Funktion, darum `::driveThrough` (Z. 219). | `setLoop`, `passVehicle`. | mittel || umgesetzt `a72a22d` |
| F-11 | `lib/schranken_app/schranken_app.cpp:389-399` | Namen | `show()` zeigt nicht nur an, sondern öffnet die Schranke, schreibt ins Ereignisprotokoll und auf die Konsole. Der Name versteckt die wichtigste Nebenwirkung. | `applyGateResult()`. | mittel || umgesetzt `a72a22d` |
| F-12 | `src/schranken/lane.h:58` | Namen | Der Debouncer der Sicherungsschleife heisst `loop`, in einer Datei mit `loop1()` und `loopPin`. | `loopDebouncer`. | tief || umgesetzt `a72a22d` |
| F-13 | `src/hwtest/main.cpp:25-26`, `32` | Magic Numbers | `write(open ? 90 : 0)` und `delay(2000)`, obwohl `BARRIER_OPEN_ANGLE`/`BARRIER_CLOSED_ANGLE` in `barrier.h` existieren. Der Kommentar Z. 6 wiederholt die Zahlen. | Konstanten verwenden, `TOGGLE_INTERVAL_MS`. | mittel || umgesetzt `c6b49df` |
| F-14 | `lib/kasse_api/kasse_api.cpp:66-225` | Duplikat | `handleEntry`, `handleExit`, `handlePay` prüfen je: Body parsen → `bad_request` → `invalid_plate` → `time_not_synced`. | Gemeinsame Vorprüfung `checkMutation()`. | mittel || offen (1) |
| F-15 | `lib/kasse_api/kasse_api.cpp`, `lib/kasse_client/kasse_client.cpp:54-85` | Duplikat | Die Zuordnung Fehlerfall ↔ (Status, Code) ist auf der Serverseite geschrieben und im Client als 7-fache `else if`-Kette nochmals umgekehrt. | Eine gemeinsame Tabelle für beide Seiten. | mittel || offen (2) |
| F-16 | `lib/kasse_api/kasse_api.cpp:52-205`, `lib/operator_view/operator_view.cpp:47-57` | Namen | Die API mischt deutsche (`"bezahlt"`, `"unbekannt"`) und englische Statuswörter (`"closed_free"`). | Ein Vokabular. | mittel || nicht umgesetzt (3) |
| F-17 | `lib/gate/gate.h:30`, `lib/html`, `lib/schranken_view` | Namen, Typen | CSS-Klasse `cls` als `const char*` (`"ok"`/`"err"`/`""`), «hat Notiz» wird über `cls[0] != '\0'` geprüft. | `enum class Tone`, `Notice::empty()`. | mittel || offen (2) |
| F-18 | `lib/kasse_view/kasse_pages.cpp:152-234` | Lange Funktion | `renderPayPage` 83 Zeilen: Einleitung, Fehler, Suchformular, Sitzung, Zahlformular, Liste. | In `appendSearchForm`, `appendSessionLookup` … aufteilen. | mittel || offen (1) |
| F-19 | `lib/operator_view/operator_view.cpp:207-280` | Lange Funktion | `remoteOpen` 74 Zeilen: Validierung, Registrierung, HTTP-Aufruf, Protokoll. | Aufteilen. | mittel || offen (1) |
| F-20 | `lib/operator_view/operator_view.cpp:174-205` | Vermischung | `laneStatus()` in einem View-Modul macht den HTTP-Aufruf und parst JSON. | Abfrage in `kasse_client`, View erhält nur Daten. | mittel || offen (1) |
| F-21 | `lib/kasse_client/kasse_client.cpp:16-88` | Lange Funktion | `paymentFromReply` 73 Zeilen. | Zusammen mit F-15 tabellengesteuert. | tief || offen (2) |
| F-22 | `lib/session/payment.h:14`, `lib/session/session.h:22` | Namen | Zwei Enums `PayMethod` und `PaymentMethod` mit ähnlicher Bedeutung, verschieden geparst. | Umbenennen nach Rolle (`PayRequestMethod`). | mittel || offen (2) |
| F-23 | `src/schranken/main.cpp:38-129` | Vermischung | `HttpKasseLink` und `BoardPlatform` (90 Zeilen Adaptercode) stehen in `main.cpp`. | Eigene Dateien in `src/schranken/`. | tief || nicht umgesetzt (4) |
| F-24 | `src/common/net.cpp:70-131` | Lange Funktion | `netUpdate` 62 Zeilen: Status loggen, Verbindung, Reconnect, NTP. | Aufteilen. | mittel || nicht umgesetzt (4) |
| F-25 | `src/common/net.cpp:44-48` | Magic Numbers | NTP-Fallback-IP und Hostnamen inline. | Konstanten. | tief || nicht umgesetzt (4) |
| F-26 | `lib/format/format.cpp:99-165`, `lib/kasse_view/kasse_pages.cpp:103` | Magic Numbers | `3600`, `7200`, `60` für Stunden/Zeitzone mehrfach; `kasse_pages` hat eigene `MIN`/`HOUR`. | `SECONDS_PER_HOUR`, `CET_OFFSET` … | tief || offen (1) |
| F-27 | `src/sim/common/http_server.cpp:51-138`, `http_client.cpp:79-175` | Lange Funktion | `readRequest` 88 und `httpRequest` 97 Zeilen (nur Simulator). | Aufteilen. | tief || offen (5) |
| F-28 | `src/sim/common/http_client.cpp:119`, `http_server.cpp:110` | Duplikat | Header `X-Api-Key` nochmals als Literal statt `API_KEY_HEADER`. | Konstante verwenden. | tief || offen (5) |
| F-29 | `lib/codec/codec.cpp:126` | Kommentar erklärt Umweg | «add() again instead of assigning, so list.revision() moves on» – fehlende Methode wird umschifft. | `RegistrationList::replaceAll()`. | tief || offen (1) |
| F-30 | `lib/auth/auth.cpp:12` | Sicherheit | Passwortvergleich nicht in konstanter Zeit (API-Schlüssel schon, `kasse_api.cpp:245`). | `apiKeyMatches` nach `auth` verschieben und für beide verwenden. | tief || nicht umgesetzt (3) |

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

## Refactoring

Branch `refactoring`, ein Commit pro Schritt, Reihenfolge von klein und risikoarm zu gross. Driver/Navigator haben
nach jedem Schritt gewechselt.

| Schritt | Commit | Findings | Driver | Navigator |
|---|---|---|---|---|
| 1 Magic Numbers im Verdrahtungstest | `c6b49df` | F-13 | | |
| 2 Eine Hex-Funktion für Token und Notiz-IDs | `68b9582` | F-06 | | |
| 3 Dateinamen, Kameraseite, Störungstext benannt | `87bf864` | F-07, F-08, F-09 | | |
| 4 Sprechende Namen in SchrankenApp und CoreLane | `a72a22d` | F-10, F-11, F-12 | | |
| 5 HTTP-Statuscodes als Konstanten | `c6e8b76` | F-04 | | |
| 6 Ein-/Ausfahrtsabfrage entdoppelt, Fehlertexte als Tabelle | `be6209c` | F-01, F-02 | | |
| 7 `Barrier::update` aufgeteilt | `d998f8c` | F-03 | | |
| 8 Formulare der Kameraseite mit einer Hilfsfunktion | `a0ff9a3` | F-05 | | |

**Regressionstest nach jedem Schritt** (Testkonzept, Abschnitt Regression):

- `pio test -e native`: alle Unit- und Systemsuites. Die Systemsuites (`test_system_entry`, `_payment`, `_safety`)
  spielen die Smoke-Testfälle TF-01, TF-04, TF-20, TF-23, TF-24 über die echten Routen durch. Die Tests wurden beim
  Refactoring **nicht** angepasst: Wenn sie unverändert grün bleiben, hat sich das Verhalten nicht geändert.
- `pio run -e kasse -e schranken -e hwtest`: die Firmware baut weiterhin (Schritte 1 und 4 betreffen `src/`).
- Schritt 8 zusätzlich: Kameraseite mit alter und neuer Version für alle 16 Kombinationen der Knöpfe gerendert,
  das HTML ist Byte für Byte gleich.
- Am Schluss: PC-Simulator (`docker compose up --build`), TF-01/04/20/23/24 im Browser.

## Vorher / Nachher

### Beispiel 1: Ausfahrt bei der Kasse abfragen (F-01, F-02)

Vorher (`lib/gate/gate.cpp`, Auszug aus 58 Zeilen `exitRequest`): Body von Hand serialisieren, den Fehlercode ein zweites
Mal selbst parsen, jeden Fehler als eigenes `if` mit nackten Statuscodes, Text «nicht erreichbar» in `terminal.cpp`
nochmals kopiert.

```cpp
    JsonDocument req;
    req["kz"] = plate;
    std::string body;
    serializeJson(req, body);

    HttpReply reply = kasse.postJson("/api/exit", body);
    ...
    std::string code = parsed && doc["error"].is<const char*>() ? doc["error"].as<std::string>() : "";
    if (reply.status == 402 && code == "unpaid" && doc["fee"].is<uint32_t>())
    {
        // 15 Zeilen Restbetrag berechnen und Text bauen
    }
    if (reply.status == 404 && code == "no_session")
    {
        return closed(plate, "Kein Parkvorgang gefunden – bitte Ruftaste drücken");
    }
    if (reply.status == 503 && code == "time_not_synced")
    {
        return closed(plate, TIME_NOT_SYNCED);
    }
    return closed(plate, UNEXPECTED);
```

Nachher: Die Fehlerfälle sind Daten, die Funktion liest sich als Ablauf.

```cpp
    const ErrorText EXIT_ERRORS[] = {
        {HTTP_NOT_FOUND, "no_session", "Kein Parkvorgang gefunden – bitte Ruftaste drücken"},
        {HTTP_UNAVAILABLE, "time_not_synced", TIME_NOT_SYNCED_TEXT},
    };
    ...
    HttpReply reply = postToKasse(kasse, "/api/exit", req);
    if (reply.status <= 0)
    {
        return kasseUnreachable(plate);
    }
    ...
    if (reply.status == HTTP_PAYMENT_REQUIRED && code == "unpaid" && doc["fee"].is<uint32_t>())
    {
        return unpaidResult(plate, doc);
    }
    if (reply.status != HTTP_OK)
    {
        return refusedByKasse(plate, reply.status, code, EXIT_ERRORS);
    }
```

`entryRequest` nutzt dieselben Hilfsfunktionen mit `ENTRY_ERRORS`. Ein neuer Fehlercode der Kasse ist jetzt eine
Tabellenzeile statt eines weiteren `if`.

### Beispiel 2: Schranke schalten (F-03)

Vorher: `Barrier::update` (56 Zeilen) verfolgt die Schleife **und** schaltet den Zustandsautomaten; im `switch` ein
`return` mitten im `case` und eine unbenannte Winkelformel.

```cpp
    case BarrierState::Closing:
        if (occupied)
        {
            reopen(nowMs);
            return BarrierEvent::SafetyStop;
        }
        uint32_t elapsed = nowMs - closingSinceMs;
        if (elapsed >= BARRIER_CLOSE_MS) { ... }
        else
        {
            currentAngle = BARRIER_OPEN_ANGLE -
                           static_cast<int>(static_cast<int64_t>(BARRIER_OPEN_ANGLE - BARRIER_CLOSED_ANGLE) * elapsed /
                                            BARRIER_CLOSE_MS);
        }
```

Nachher: `update` sagt in drei Zeilen, was passiert; jede Teilfunktion hat eine Aufgabe.

```cpp
BarrierEvent Barrier::update(uint32_t nowMs, bool loopOccupied)
{
    BarrierEvent loopEvent = trackLoop(nowMs, loopOccupied);
    BarrierEvent stateEvent = stepStateMachine(nowMs);
    return stateEvent != BarrierEvent::None ? stateEvent : loopEvent;
}
```

`stepClosing()` enthält den Sicherheitsstopp, `sweepAngle()` die Winkelformel. Dass ein Sicherheitsstopp eine
gleichzeitige Störmeldung übersteuert, war vorher nur am frühen `return` erkennbar, jetzt steht es in `update`.

## Nicht umgesetzt

1. **Lange Funktionen und Duplikate in Kasse, Automat, Betrieb** (F-14, F-18, F-19, F-20, F-26, F-29): sinnvoll, aber
   tiefere Priorität als die Schranken- und Gate-Logik (Testziele 01, 03, 07 mit Prio 1). Mit 6–8 Schritten in
   der verfügbaren Zeit haben wir die sicherheitsrelevanten Teile vorgezogen.
2. **Querschnitts-Umbauten** (F-15, F-17, F-21, F-22): Eine gemeinsame Fehlertabelle für API und Client oder ein `enum`
   statt `cls` betrifft fast alle Views und viele Tests. Das ist kein «kleiner Schritt» mehr und gehört in einen
   eigenen Branch.
3. **Verhaltensänderungen** (F-16, F-30, B-01): Andere Statuswörter in der API ändern die Schnittstelle zwischen den
   Picos; ein Passwortvergleich in konstanter Zeit und die 403-Statuszeile ändern das Verhalten. Das ist kein
   Refactoring und gehört als `fix:` auf `main`.
4. **Board-Code in `src/`** (F-23, F-24, F-25): `main.cpp`, `net.cpp` laufen nur auf dem Pico und haben keine Unit-Tests.
   Ohne Hardware können wir nur prüfen, dass es baut, nicht dass WLAN und NTP noch funktionieren. Das Risiko ist
   grösser als der Nutzen.
5. **Simulator** (F-27, F-28): nur Testhilfe, nicht Teil des Produkts.

## Reflexion

**Was hat geholfen?**

- **Tests:** 22 native Suites, davon drei Systemsuites über die echten Routen. Jeder Schritt war in Sekunden
  geprüft; ohne diese Tests hätten wir `Barrier::update` (Sicherheitslogik) nicht angefasst.
- **Logik ohne Hardware:** Weil `lib/` kein `Arduino.h` kennt und die Zeit als Parameter bekommt, liess sich die
  Schranke auf dem PC testen. Die Trennung, die der Auftrag als typisches Problem nennt, hatten wir schon.
- **Kleine Commits:** Jeder Schritt ist einzeln nachvollziehbar und wäre einzeln rückgängig zu machen.

**Was hat es erschwert?**

- **Keine Hardware:** Code in `src/` (Servo, WLAN, Kern 1) können wir nur bauen, nicht laufen lassen; darum blieb er
  grösstenteils unverändert (siehe Punkt 4 oben).
- **Exakte UI-Texte:** Die Tests vergleichen die deutschen Texte aus dem Testkonzept wörtlich. Beim Entdoppeln
  (F-01, F-05) mussten Texte und HTML Byte für Byte gleich bleiben, z. B. das fehlende Leerzeichen nach der Ruftaste.
- **Code war schon recht sauber:** Viele Punkte der Checkliste (grosses `loop()`, toter Code, Pins im Code) trafen
  nicht zu. Die Findings waren eher klein; das Review musste genauer hinschauen.
