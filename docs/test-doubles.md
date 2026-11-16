# Abhängigkeiten der Logik und Test Doubles

Analyse, welche Abhängigkeiten die Logik des Parkhaus-Systems hat, warum sie in automatisierten Tests nicht
(oder nur schlecht) verwendet werden können und durch welche Test Doubles sie ersetzt werden.

## Grundprinzip

Die ganze Logik liegt in `lib/` und bindet weder `Arduino.h` noch Hardware-Funktionen (`millis()`, `Serial`,
`digitalRead()`, LittleFS, HTTPClient) ein. Sie läuft deshalb unverändert auf dem PC (`pio test -e native`).
Abhängigkeiten erreicht die Logik nur auf zwei Wegen:

- **Über Schnittstellen (Ports):** `KasseLink` (`lib/gate/gate.h`), `BarrierPort` und `AppPlatform`
  (`lib/schranken_app/schranken_app.h`). Die echten Implementierungen sind die Adapter in `src/` (Board) und
  `src/sim/` (PC-Simulator); in den Tests werden Test Doubles eingesetzt.
- **Als Parameter:** Uhrzeit (`now`, `nowMs`), NTP-Status (`timeSynced`), Absender-IP (`fromSchrankenIp`),
  API-Schlüssel und Betreiber-Passwort. Hier braucht es kein Double-Objekt; der Test übergibt einfach den
  gewünschten Wert.

Die Test Doubles liegen in `lib/test_support/test_fixtures.h` (gemeinsam genutzt),
`lib/test_support/system_rig.h` (Systemtest) und direkt in einzelnen Suites unter `test/native/`.

## Übersicht

| # | Abhängigkeit | Schnittstelle | Echte Implementierung | Test Double(s) | Art |
|---|---|---|---|---|---|
| 1 | Pico 1 (Kasse) über HTTP/WLAN | `KasseLink` | HTTPClient (`src/schranken/main.cpp`), Socket-Client im Simulator | `FixedReply`, `BrokenKasse`, `FakeKasse`, `LocalKasse` | Stub, Stub + Spy, Fake |
| 2 | Schranke: Servo + Sicherungsschleife (Core 1) | `BarrierPort` | `CoreLane` (`src/schranken/lane.*`) | `FakeBarrier`, `LocalBarrier` | Stub + Spy, Fake |
| 3 | Flash-Speicher (LittleFS) auf Pico 2 | `AppPlatform::readFile/writeFile` | `src/common/storage.cpp` | `MemoryPlatform` | Fake + Spy |
| 4 | Zufallszahlen (Login-Token, Meldungs-IDs) | `AppPlatform::random32` | Hardware-Zufall des Pico | `MemoryPlatform::random32` | Stub |
| 5 | Konsolen-Log (`Serial`) | `AppPlatform::log` | `Serial.println` | `MemoryPlatform::log` (leer) | Dummy |
| 6 | Uhrzeit (NTP-Zeit, `millis()`) | Parameter `now`, `nowMs` | `nowUtc()`, `millis()` | feste Werte, simulierte Uhr im `SystemRig` | Stub-Werte / Fake-Uhr |
| 7 | NTP-Status | Parameter `timeSynced` | `timeReady()` | `LocalKasse::synced` | Stub-Wert |
| 8 | Absender-IP, API-Schlüssel, Passwort (`secrets.h`) | Parameter | `secrets.h`, `remoteIP()` | Testwerte (`"test-api-key"`, `"geheim"`, `true/false`) | Dummy / Stub-Werte |
| 9 | Webserver / Browser | `WebRequest` / `WebResponse` | `src/common/web_glue.cpp`, Simulator-Server | – (Datenstrukturen direkt) | kein Double nötig |
| 10 | Flash-Speicher auf Pico 1 | Rückgabe `changed` von `routeKasse` | `src/kasse/main.cpp` | – (echter `codec`, JSON im Speicher) | kein Double nötig |

## 1. Pico 1 (Kasse) über HTTP/WLAN — `KasseLink`

Pico 2 fragt Pico 1 bei Einfahrt, Ausfahrt, Zahlung und für die Session-Liste per HTTP/JSON an.

**Warum nicht im Test verwendbar?**
- Aufwändig: braucht einen zweiten Pico (oder den laufenden Simulator) und ein WLAN.
- Langsam: ein ausbleibender Pico 1 wird erst nach dem Timeout von 1000 ms erkannt.
- Nicht deterministisch: Netzwerk-Latenz und Paketverluste sind zufällig.
- Schwierig: Fehlerfälle (Timeout, `503 time_not_synced`, kaputte Antwort) lassen sich mit dem echten Pico 1 kaum
  gezielt und wiederholbar auslösen — gerade diese Fälle sind aber sicherheitsrelevant («Schranke bleibt zu»).

**Welches Verhalten braucht die Logik wirklich?**
Nur zwei Methoden: `postJson(path, body)` und `getJson(pathAndQuery)`, die einen HTTP-Status und einen Body
zurückgeben — oder «keine Antwort» (`status <= 0`). Wie die Anfrage transportiert wird, ist der Logik egal.

**Test Doubles und Begründung**
- **Stub — `FixedReply`** (`test_fixtures.h`) und **`BrokenKasse`** (`test_operator_view`): liefern immer dieselbe
  vorgegebene Antwort. Geeignet für Fehlerpfade, bei denen nur die Reaktion der Logik auf eine bestimmte Antwort
  zählt, z. B. `test_fetch_sessions_unreachable_or_garbage` (unlesbare Antwort) oder
  `test_remote_open_unexpected_reply_stays_closed` (503 → Schranke bleibt zu).
- **Stub + Spy — `FakeKasse`** (`test_gate`): gibt eine vorgegebene Antwort zurück *und* zeichnet `lastPath`,
  `lastBody` und die Anzahl `calls` auf. Der Spy-Teil ist nötig, weil manche Anforderungen die Interaktion selbst
  betreffen: `test_tf07_entry_unregistered_stays_closed_without_request` prüft, dass bei einem nicht registrierten
  Kennzeichen **gar keine** Anfrage an Pico 1 geht (`calls == 0`), und `test_tf04_tf05_entry_registered_opens`
  prüft den gesendeten Pfad und JSON-Body. (Der Name «FakeKasse» ist historisch; fachlich ist es ein Stub mit
  Spy-Funktion.)
- **Fake — `LocalKasse`** (`test_fixtures.h`): eine funktionierende, vereinfachte Kasse. Sie leitet die Anfrage
  direkt an den echten Router `routeKasse` mit einem echten `SessionStore` im Arbeitsspeicher weiter — ohne Netz.
  So testen die Systemtests das Zusammenspiel beider Picos mit echter Gebührenberechnung. Mit `online = false`
  simuliert sie einen Timeout (z. B. `test_tf30_kasse_off_at_exit_logged_and_paid_status_kept`), mit
  `synced = false` eine nicht synchronisierte Uhr. Der Zähler `calls` dient zusätzlich als Spy.

Ein Fake statt vieler Stubs für die Systemtests, weil dort ganze Abläufe (Einfahrt → Zahlung → Ausfahrt) laufen
und ein Stub jede Antwort von Hand vorgeben müsste; das wäre aufwändig und würde die echte Kassenlogik umgehen.

## 2. Schranke: Servo und Sicherungsschleife — `BarrierPort`

Auf dem Board läuft jede Schranke (Servo + Schleifen-Pin) auf Core 1 (`CoreLane`), die App auf Core 0 gibt nur
Befehle.

**Warum nicht im Test verwendbar?**
- Aufwändig: braucht Pico, Servos, Taster/IR-Sensor und Stromversorgung.
- Langsam: Mindestöffnungszeit 5 s, Schliessen 2 s, Schleifenstörung erst nach 5 min.
- Nicht deterministisch: zwei Prozessorkerne laufen parallel, das Timing zwischen Befehl und Ausführung schwankt.
- Schwierig: «Fahrzeug unter der Schranke während des Schliessens» müsste physisch im richtigen Moment ausgelöst
  werden.

**Welches Verhalten braucht die Logik wirklich?**
Zustand lesen (`state()`: offen / schliesst / geschlossen), Schleifenstörung und «Schleife gehalten» lesen,
`requestOpen()`, `requestPass()` (mit Antwort, ob die Durchfahrt akzeptiert wurde) und `setLoopHeld()`.

**Test Doubles und Begründung**
- **Stub + Spy — `FakeBarrier`** (`test_schranken_app`): Der Test setzt Zustand, Störung und die Antwort von
  `requestPass()` direkt (Stub) und liest danach die Zähler `opens` / `passes` (Spy). Die App-Tests prüfen nur die
  *Entscheidung* der App («öffnen ja/nein, genau einmal»), nicht die Mechanik, z. B.
  `test_entry_opens_and_terminal_shows_plate` (`opens == 1`), `test_no_capture_while_barrier_up` und
  `test_drive_through_uses_the_barrier_answer`.
- **Fake — `LocalBarrier`** (`lib/schranken_app/local_barrier.h`): eine echte, aber vereinfachte Schranke: die
  echte Zustandsmaschine `Barrier` mit einer simulierten Millisekunden-Uhr, ohne Servo; die Schleife ist ein Flag
  («Fahrzeug bleibt stehen»). Sie wird in den Systemtests und im PC-Simulator verwendet, weil dort das
  Zeitverhalten getestet wird: `test_tf13_tf14_stop_while_closing_and_six_log_entries` (Sicherheitsabschaltung
  mitten im Schliessen) und `test_tf15_loop_fault_after_five_minutes` laufen in Millisekunden statt Minuten.

Die Zustandsmaschine `Barrier` selbst braucht kein Double: sie bekommt Zeit und Schleifenzustand als Parameter
(`update(nowMs, loopOccupied)`) und wird in `test_barrier` direkt getestet.

## 3. Flash-Speicher auf Pico 2 — `AppPlatform::readFile/writeFile`

Registrierungen und Ereignisprotokoll werden als JSON in LittleFS gespeichert.

**Warum nicht im Test verwendbar?**
- LittleFS gibt es nur auf dem Board.
- Keine Isolation: Dateien bleiben zwischen Tests bestehen, ein Test würde den nächsten beeinflussen.
- Flash hat begrenzte Schreibzyklen und ist langsam.

**Welches Verhalten braucht die Logik wirklich?**
Eine Datei per Name lesen (oder melden, dass sie fehlt) und eine Datei per Name schreiben.

**Test Double und Begründung**
- **Fake + Spy — `MemoryPlatform`** (`test_fixtures.h`): speichert die Dateien in einer `std::map` statt im Flash
  (Fake — funktioniert wirklich, nur einfacher). Ein Neustart (`SystemRig::rebootSchranken()`) baut die App aus
  derselben Map neu auf; so prüft `test_tf01_register_survives_reboot`, dass Daten einen Neustart überleben.
  Zusätzlich zählt `writes` die Schreibvorgänge pro Datei (Spy): `test_saves_only_what_changed` prüft, dass nur
  geänderte Dateien geschrieben werden (schont den Flash).

## 4. Zufallszahlen — `AppPlatform::random32`

Login-Token (128 bit) und die IDs der Meldungen nach einem Formular (`?m=<id>`) sind zufällig.

**Warum nicht im Test verwendbar?** Nicht deterministisch: Cookie-Werte und Redirect-URLs wären bei jedem Lauf
anders, ein Fehler liesse sich nicht reproduzieren.

**Welches Verhalten braucht die Logik wirklich?** Irgendeinen 32-bit-Wert, der sich von Aufruf zu Aufruf
unterscheidet.

**Test Double und Begründung**
- **Stub — `MemoryPlatform::random32`**: gibt fortlaufend `0x1000`, `0x1001`, … zurück. Deterministisch und
  trotzdem eindeutig, also reicht ein Stub. Die Qualität des echten Zufalls ist kein Testziel der Logik.

## 5. Konsolen-Log — `AppPlatform::log`

**Warum nicht im Test verwendbar?** `Serial` gibt es nur auf dem Board; die Ausgabe ist im Test bedeutungslos.

**Welches Verhalten braucht die Logik wirklich?** Keines — die Methode muss nur aufrufbar sein.

**Test Double und Begründung**
- **Dummy — `MemoryPlatform::log`** (leere Methode): wird nur gebraucht, damit die Schnittstelle erfüllt ist.
  Was wirklich geprüft werden muss (Ereignisse wie «Kasse nicht erreichbar»), steht im `EventLog` und wird dort
  direkt abgefragt.

## 6. Uhrzeit — Parameter `now` und `nowMs`

Pico 1 rechnet Gebühren mit der NTP-Zeit (Unix-Sekunden), die Schranke arbeitet mit `millis()`.

**Warum nicht im Test verwendbar?**
- Langsam: Tarife hängen von Stunden und Tagen Parkzeit ab; Freizeit, Tagesblöcke und Höchstbeträge lassen sich
  mit Echtzeit nicht testen.
- Nicht deterministisch: die echte Zeit ändert sich während des Tests; Grenzwerte «auf die Sekunde» wären nicht
  reproduzierbar.
- Schwierig: Tageswechsel und Sommer-/Winterzeit lassen sich nicht abwarten.

**Welches Verhalten braucht die Logik wirklich?** Nur die aktuelle Zeit als Zahl.

**Test Double und Begründung**
Weil die Zeit als **Parameter** übergeben wird (Design-Entscheid «Zeit ist ein Parameter»), braucht es kein
Double-Objekt. Unit-Tests übergeben feste Werte (`fixtures::NOW`, `NOW + 2 * HOUR`) — das entspricht einem
**Stub-Wert**, z. B. `test_reference_values` in `test_tariff`. Der Systemtest hat eine **simulierte Uhr** (Fake):
`SystemRig::wait(seconds)` und `waitMs(ms)` stellen die Zeit vor und lassen dabei die Schranken laufen, und
`entryAgo()` datiert eine Einfahrt zurück. So läuft `test_tf18_reference_fees_to_the_second` in Millisekunden.

## 7. NTP-Status — Parameter `timeSynced`

**Warum nicht im Test verwendbar?** Den Zustand «Uhr nicht synchronisiert» kann man auf dem echten Board nur
erzeugen, indem man das Internet trennt und neu startet — aufwändig und nicht automatisierbar.

**Welches Verhalten braucht die Logik wirklich?** Ein `bool`.

**Test Double und Begründung** Ein fester **Stub-Wert** (`LocalKasse::synced = false` oder direkt `false` an
`routeKasse`), damit die Antwort `503 time_not_synced` und die Meldung auf Pico 2 testbar sind.

## 8. Absender-IP, API-Schlüssel und Passwort

Pico 1 akzeptiert ändernde Anfragen nur von der IP von Pico 2 mit dem richtigen `X-Api-Key`; die Betreiberseiten
brauchen ein Passwort. Die echten Werte stehen in `include/secrets.h` (nicht im Git).

**Warum nicht im Test verwendbar?** Echte Geheimnisse gehören nicht in Tests und Repository; eine gefälschte
Absender-IP lässt sich im Test nicht über ein echtes Netz erzeugen.

**Welches Verhalten braucht die Logik wirklich?** Einen Wahrheitswert (`fromSchrankenIp`) und Zeichenketten zum
Vergleichen.

**Test Double und Begründung** Testwerte als Parameter: `API_KEY = "test-api-key"`, Passwort `"geheim"` /
`"parkhaus"`. Wo sie nur die Signatur füllen, sind es **Dummies**; wo der Test einen Fall gezielt herstellt, sind
es **Stub-Werte**, z. B. `test_tf39_spoofed_ip_without_key_is_forbidden` (`fromSchrankenIp = true`, aber falscher
Schlüssel → 403) und `test_route_kasse_writes_only_from_pico2`.

## 9. Webserver und Browser — `WebRequest` / `WebResponse`

**Warum nicht im Test verwendbar?** Ein echter Server mit Socket und Browser wäre langsam, braucht Ports und ist
für automatisierte Tests unnötig kompliziert.

**Welches Verhalten braucht die Logik wirklich?** Methode, Pfad, Parameter und Cookie hinein; Status, Body,
`Location` und `Set-Cookie` heraus.

**Kein Double nötig:** Die Logik arbeitet nur mit den Datenstrukturen `WebRequest` und `WebResponse`. Der Adapter
in `src/` übersetzt lediglich zwischen Server und diesen Strukturen und enthält keine Logik («Humble Object»). Die
Tests bauen die Strukturen direkt; `SystemRig` spielt den Browser (`request`, `submit` folgt dem Redirect wie ein
Browser).

## 10. Flash-Speicher auf Pico 1

**Kein Double nötig:** `routeKasse` speichert nicht selbst, sondern meldet nur `changed = true`; das Schreiben
macht `src/kasse/main.cpp`. Den Neustart von Pico 1 testet `SystemRig::rebootKasse()` mit dem echten `codec`:
Sessions → JSON-Text → neuer `SessionStore`.

## Fazit: nach welcher Regel wir die Art wählen

| Art | Wann wir sie einsetzen | Beispiele |
|---|---|---|
| **Dummy** | Nur die Signatur muss erfüllt sein, das Verhalten ist egal | `MemoryPlatform::log`, Passwort in Tests ohne Login |
| **Stub** | Die Logik soll auf eine bestimmte, vorgegebene Antwort reagieren (v. a. Fehlerpfade) | `FixedReply`, `BrokenKasse`, `random32`, `synced = false` |
| **Spy** | Die Interaktion selbst ist das Testziel (wie oft, was gesendet) | `FakeKasse` (`calls`, `lastBody`), `FakeBarrier` (`opens`), `MemoryPlatform::writes` |
| **Fake** | Ganze Abläufe brauchen realistisches Verhalten, aber ohne Hardware/Netz | `LocalKasse`, `LocalBarrier`, `MemoryPlatform`-Dateien, simulierte Uhr im `SystemRig` |
| **Mock** | — bewusst nicht verwendet | — |

**Warum keine Mocks?** Ein Mock prüft die erwarteten Aufrufe selbst (vorab festgelegte Erwartungen) und braucht
dafür meist ein Framework. Unity bietet keines, und unsere Spies plus normale `TEST_ASSERT`s decken dieselben Fälle
ab: Der Test zeichnet auf und prüft danach. Das ist lesbarer und koppelt die Tests weniger an die genaue
Aufrufreihenfolge, sodass interne Umbauten die Tests nicht unnötig brechen.
