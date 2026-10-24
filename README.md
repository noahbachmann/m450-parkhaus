# Parkhaus mit Schranke

Projekt im Modul 450 (BBZW Sursee, Klasse sina24cL).

## Gruppenmitglieder

- Noah Bachmann
- Paul Willimann

## Kurzbeschreibung der Umsetzung

Prototyp eines Parkhauses mit Ein- und Ausfahrtsschranke auf zwei Raspberry Pi Pico W, programmiert in C++ mit
dem Arduino-Framework und gebaut mit PlatformIO. Die beiden Picos sprechen über ein eigenes WLAN per HTTP
miteinander.

```
  Browser (Laptop/Handy)
        │ alle Seiten
        ▼
┌──────────────────────────────┐   HTTP/JSON    ┌──────────────────────────────┐
│ Pico 2 «Schranken»           │ ─────────────► │ Pico 1 «Kasse»               │
│ • Ein-/Ausfahrtsschranke     │  Einfahrt,     │ • Parkvorgänge (Flash)       │
│   (2 Servos, 2 Schleifen)    │  Ausfahrt,     │ • Gebührenberechnung         │
│ • Registrierung, Ereignis-   │  Zahlung,      │ • Zahlungen (Karte, TWINT,   │
│   protokoll                  │  Status        │   Bargeld mit Rückgeld)      │
│ • Weboberfläche: Kamera-     │                │ • JSON-Schnittstelle         │
│   simulation, Automat, Tarif,│                │                              │
│   Kasse, Registrierung,      │                │                              │
│   Leitstelle                 │                │                              │
└──────────────────────────────┘                └──────────────────────────────┘
```

- **Einfahrt:** Das Kennzeichen wird in der Kamerasimulation eingegeben. Ist es registriert, eröffnet Pico 1
  einen Parkvorgang mit Zeitstempel und die Schranke öffnet.
- **Gebühr und Bezahlung:** 15 Freiminuten, danach CHF 2.00 pro angefangene Stunde ab Einfahrt, höchstens
  CHF 20.00 pro angefangene 24 Stunden. Bezahlt wird am Automaten (ohne Anmeldung), an der Kasse
  (Betriebspersonal) oder mit Karte/TWINT direkt am Ausfahrtsterminal. Die Zahlungsmittel sind simuliert.
- **Ausfahrt:** Pico 2 fragt Pico 1, ob die Gebühr bezahlt ist (oder CHF 0.00 beträgt). Nur dann öffnet die
  Schranke und der Parkvorgang wird abgeschlossen. Antwortet Pico 1 nicht, bleibt die Schranke zu.
- **Sicherheit:** Die Schranke schliesst erst, wenn das Fahrzeug durchgefahren und die Sicherungsschleife frei
  ist. Wird die Schleife beim Schliessen belegt, fährt der Arm sofort wieder hoch. Die Schranken laufen auf dem
  zweiten Prozessorkern von Pico 2, damit eine langsame Antwort der Kasse nie einen Stopp verzögert.
- **Leitstelle:** Ruftaste, Fernöffnung nur mit Grund, Ereignisprotokoll (Fernöffnungen,
  Sicherheitsabschaltungen, Störungen).

Die ganze Logik liegt in `lib/` und ist ohne Hardware testbar. `src/` enthält nur die Anbindung an die Picos
bzw. an den PC-Simulator, der dieselbe Logik ohne Hardware ausführt.

## Umgesetzte User Stories / Anforderungen

### Auftrag (Minimalumfang)

| Anforderung | Status |
|---|---|
| Ein- und Ausfahrtsschranke mit zwei Raspberry Pi Pico (Arduino) | umgesetzt (Firmware gebaut, auf Hardware nicht getestet, siehe Einschränkungen) |
| Berechnung der Parkgebühr | umgesetzt |
| Bezahlung der Parkgebühr | umgesetzt (Automat, Kasse, Ausfahrtsterminal) |
| README mit Gruppenmitgliedern, Beschreibung, User Stories | dieses Dokument |

### User Stories

Grundlage: «Parksystem – Anforderungen» (US-01 bis US-22). Die User Stories sind für ein echtes Parkhaus mit
Kameras, App, Sensoren und Zahlungsdienstleister geschrieben; der Prototyp setzt um, was mit zwei Picos möglich
ist (gleiche Abgrenzung wie im Testkonzept, Kapitel 2).

| ID | Titel | Status | Umsetzung / Begründung |
|---|---|---|---|
| US-01 | Registrierung per App oder Desktop | teilweise | Registrierung über die Weboberfläche mit Name und Kennzeichen, keine Duplikate (AK2, AK4). Keine App, keine E-Mail-Bestätigung (AK1, AK3). |
| US-02 | Schnelle Registrierung an der Einfahrt (QR-Code) | nicht umgesetzt | Widerspricht dem Auftrag (Einfahrt nur mit registriertem Kennzeichen). Ersatz: Die Leitstelle kann an der Einfahrt «Neuer Kunde registrieren». |
| US-03 | Belegungssensoren, freie Plätze | nicht umgesetzt | Keine Platzsensoren, LEDs oder Anzeigetafel im Prototyp. |
| US-04 | Freie Plätze in der App | nicht umgesetzt | Keine App. |
| US-05 | Parkplätze nach Art filtern | nicht umgesetzt | Keine App. |
| US-06 | Einfahrtsschranke öffnet automatisch | umgesetzt | Kennzeichen in der Kamerasimulation (AK1), Öffnen für registrierte Kennzeichen (AK2), Rückmeldung mit Kennzeichen am Terminal (AK4). Statt Erkennungsquote (AK3) wird die Normalisierung der Schreibweise geprüft. |
| US-07 | Parkuhr startet bei der Einfahrt | umgesetzt | Parkvorgang mit sekundengenauem Zeitstempel ohne Eingabe (AK1, AK2), höchstens einer pro Kennzeichen (AK4), Zeit von Pico 1 (AK5). Laufende Dauer an Kasse und Automat statt in der App (AK3). |
| US-08 | Hilfe bei nicht erkanntem Kennzeichen | umgesetzt (ohne AK1) | Ruftaste an beiden Terminals; die Leitstelle sieht Spur, Kennzeichen, Zahlungsstatus (AK2), öffnet nur mit Grund aus einer Liste (AK3), an der Einfahrt mit Parkvorgang (AK4), alles protokolliert (AK5). Keine Sprechverbindung (AK1). |
| US-09 | Sichere Schrankenbewegung | umgesetzt (ohne AK5) | Schliesst erst bei freier Schleife (AK1), Stopp und ganz hoch beim Schliessen (AK2), Störung nach 5 min (AK3), Protokoll (AK4). Konformitätsnachweis SN EN 12453 (AK5) betrifft eine echte Anlage. |
| US-10 | Wegweiser und Pläne | nicht umgesetzt | Bauliche Massnahme. |
| US-11 | Nummerierte Parkplätze | nicht umgesetzt | Bauliche Massnahme bzw. App. |
| US-12 | Behindertenparkplätze | nicht umgesetzt | Bauliche Massnahme bzw. App. |
| US-13 | Automatische Erkennung bei der Ausfahrt | umgesetzt | Parkvorgang wird ermittelt und mit der Ausfahrtszeit beendet (AK1, AK2); ohne Parkvorgang bleibt die Schranke zu, Hinweis auf die Ruftaste (AK3); «Fahrzeug wendet» lässt ihn offen (AK4). |
| US-14 | Parkdauer und Kosten nachvollziehen | umgesetzt | Dauer, Betrag, angefangene Stunden und Freiminuten an Kasse und Automat (AK1, AK2), Tarifseite (AK3). Keine App. |
| US-15 | Hinterlegte Zahlungsmethode belasten | nicht umgesetzt | Keine App, kein Zahlungsdienstleister. |
| US-16 | Ausfahrt bei fehlgeschlagener Zahlung verhindern | umgesetzt | Öffnet nur bezahlt oder bei CHF 0.00 (AK1); Ablehnung mit Grund (AK2); anderes Zahlungsmittel am Ausfahrtsterminal (AK3); nach 5 Ablehnungen wird die Leitstelle benachrichtigt (AK4); jede Öffnung ohne Zahlung mit Grund und Benutzer protokolliert (AK5). |
| US-17 | Bezahlen am Automaten über das Kennzeichen | umgesetzt | Öffentliche Seite «Automat»: Karte, TWINT, Bargeld mit Rückgeld (AK1, simuliert), danach sofort «bezahlt» (AK2). |
| US-18 | Quittung erhalten | nicht umgesetzt | Keine App, kein Drucker. Die Transaktionsnummer wird angezeigt. |
| US-19 | Tarife selbst konfigurieren | nicht umgesetzt | Tarif fest im Code (`lib/tariff`), auf der Tarifseite einsehbar. |
| US-20 | Auswertungen zu Auslastung und Umsatz | nicht umgesetzt | Kein Backoffice. |
| US-21 | Störungen automatisch melden | teilweise | Störungen (Schleife, Kasse nicht erreichbar, 5 Ablehnungen) erscheinen in der Leitstelle und im Ereignisprotokoll. Kein E-Mail/SMS-Versand. |
| US-22 | Ausfahrt bei Stromausfall und Brandalarm | nicht umgesetzt | Keine Brandmeldeanlage; ein stromloser Servo kann die Schranke nicht öffnen. Nach einem Neustart sind alle Parkvorgänge noch da. |

## Abweichungen vom Testkonzept

| Testkonzept | Umsetzung | Grund |
|---|---|---|
| Kasse- und Automatenseite auf Pico 1 | Alle Seiten auf Pico 2; Pico 1 bietet nur die Schnittstelle | Eine Oberfläche mit einer Navigation (Befund aus TF-34 Runde 1). |
| Kasse für alle zugänglich | Kasse nur nach Anmeldung als «betrieb»; der Automat ist öffentlich | Die Kasse zeigt alle Parkvorgänge; Kundinnen und Kunden bezahlen am Automaten. |
| Bezahlen nur an Kasse/Automat | Zusätzlich Karte/TWINT am Ausfahrtsterminal | US-16 AK3 (TF-28). |
| TF-10: zweite Einfahrt wird abgewiesen | Eine Wiederholung innert 30 s für einen offenen, unbezahlten Parkvorgang gilt als dieselbe Einfahrt und öffnet | Antwortet Pico 1 erst nach dem Timeout (1 s), existiert der Parkvorgang schon, obwohl die Schranke zu blieb. TF-10 wird mit mindestens 30 s Abstand durchgeführt. |
| Sicherungsschleife mit Taster/IR-Sensor | Zusätzlich «Fahrzeug bleibt stehen» auf der Kamerasimulation; «Durchfahren» ersetzt das Überfahren der Schleife | Im Simulator und ohne verdrahtete Taster prüfbar. |
| Systemtest am Aufbau (TU-2) | Systemtest auf dem PC über dieselben Seiten und Formulare (`test/native/test_system_*`) | Die Picos standen nicht mehr zur Verfügung. Zeitmessungen sind daher offen. |

## Weboberflächen

Alle Seiten laufen auf Pico 2 (`http://<SCHRANKEN_IP>`, im Simulator `http://localhost:8082`). Ohne Anmeldung:
Kamerasimulation, Automat, Tarif. Nach der Anmeldung als «betrieb» (Passwort aus `secrets.h`, im Simulator
`parkhaus`) zusätzlich Kasse, Registrierung und Betrieb.

| Adresse | Zugang | Inhalt |
|---|---|---|
| `/kamera` (Startseite) | alle | Kennzeichen an Ein- und Ausfahrt eingeben (Einfahrt optional rückdatiert), «Durchfahren», «Fahrzeug bleibt stehen», Ruftaste, «Fahrzeug wendet», Bezahlen am Ausfahrtsterminal; darunter alle Parkvorgänge als Testhilfe |
| `/automat` | alle | Kennzeichen suchen, Dauer und Gebühr, bezahlen mit Karte, TWINT oder Bargeld; zeigt nur den gesuchten Parkvorgang |
| `/tarif` | alle | gültiger Tarif mit Beispielen |
| `/login` | alle | Anmeldung Betriebspersonal |
| `/kasse` | Betrieb | jedes Kennzeichen suchen (auch abgeschlossene Parkvorgänge), bezahlen, Liste aller Parkvorgänge |
| `/registrierung` | Betrieb | Kennzeichen registrieren und löschen |
| `/betrieb` | Betrieb | Leitstelle: Spuren, Fernöffnung mit Grund (an der Einfahrt auch «Neuer Kunde registrieren»), Ereignisprotokoll mit Filter, Testdaten zurücksetzen |

Pico 1 (`http://<KASSE_IP>`, im Simulator `http://localhost:8081`) zeigt unter `/` nur einen Link auf Pico 2 und
bietet die JSON-Schnittstelle für Pico 2: `GET /api/status?kz=…` (z. B. für TF-29 mit curl), `GET /api/sessions`,
`POST /api/entry`, `/api/exit`, `/api/pay`, `/api/reset`. Ändernde Anfragen (POST) nimmt Pico 1 nur von Pico 2 an.

### Bedienung der Kamerasimulation

- Die Schranke schliesst erst, wenn das Fahrzeug durchgefahren ist: «Durchfahren» (oder eine angeschlossene
  Schleife, die belegt und wieder frei wird), frühestens 5 s nach dem Öffnen.
- «Fahrzeug bleibt stehen» belegt die Sicherungsschleife, solange die Schranke offen ist oder schliesst;
  «Fahrzeug fährt weiter» gibt sie frei und zählt als Durchfahrt.
- Sicherheitsabschaltung (TF-13) von Hand: Schranke öffnen, nach mindestens 5 s «Durchfahren», während der
  2 s Schliessen «Fahrzeug bleibt stehen» klicken. Die Schranke ist wieder offen, im Ereignisprotokoll steht
  «Sicherheitsabschaltung».
- Die Seite aktualisiert sich nicht von selbst: «Aktualisieren» bzw. neu laden.

## Build und Tests

```
pio run                          # Firmware beider Picos bauen (kasse, schranken)
pio run -e kasse -t upload       # Pico 1 flashen (Pico 2: -e schranken)
pio run -e hwtest -t upload      # Verdrahtungstest für Pico 2 (Servos, Schleifen)
pio device monitor -b 115200     # serielle Konsole
pio test -e native               # Unit- und Systemtests auf dem PC (ohne Board)
```

Hardware-Aufbau, Pinbelegung, WLAN-Einrichtung und Flashen: [docs/aufbau-anleitung.md](docs/aufbau-anleitung.md).

### Tests

- **Unit-Tests** pro Bibliothek in `test/native/test_<name>/` (Tarif mit den Referenzwerten R-01 bis R-12
  auf die Sekunde, Kennzeichen, Parkvorgänge, Zahlung, Schnittstelle, Seiten, Schranke, Anmeldung …).
- **Systemtests** nach Kapiteln des Testkonzepts: `test_system_entry` (Registrierung, Einfahrt),
  `test_system_safety` (Schrankensicherheit, Fernöffnung), `test_system_payment` (Gebühr, Bezahlung, Ausfahrt,
  Gesamtablauf). Sie bedienen Pico 2 über dieselben Seiten und Formulare wie der Browser; Pico 1 antwortet über
  dieselbe Schnittstelle wie auf dem Board.
- Ergebnisse, Mängel und Testbericht: [docs/testprotokoll.md](docs/testprotokoll.md).

### PC-Simulator (ohne Hardware)

Beide Picos laufen als zwei Programme auf dem PC, mit derselben Logik aus `lib/` wie die Firmware.

```
docker compose up --build        # mit Docker (empfohlen), danach http://localhost:8082
pio run -e sim_kasse -t exec     # ohne Docker, zweites Terminal: pio run -e sim_schranken -t exec
```

- Pico 1 «ausschalten» (TF-11, TF-30): `docker compose stop kasse`, wieder einschalten mit
  `docker compose start kasse`.
- Daten liegen in `sim-data/` (gleiches JSON-Format wie im Flash der Picos); Ordner löschen = Testdaten.
- Die Konsole zeigt alle Anfragen, Entscheide und Schrankenbewegungen. Die Uhr ist die Uhr des PCs.

## Projektstruktur

| Ordner | Inhalt |
|---|---|
| `lib/` | gesamte Logik, eine Bibliothek pro Ordner; alle Seiten und Routen von Pico 2 in `lib/schranken_app`, die von Pico 1 in `lib/kasse_api` |
| `src/kasse/`, `src/schranken/`, `src/common/` | Anbindung an die Picos (WLAN, Webserver, Flash, Servos) |
| `src/sim/` | Anbindung an den PC (Simulator) |
| `test/native/` | Unit- und Systemtests |
| `docs/` | Aufbauanleitung, Testprotokoll; `docs/context/` = Spezifikation und Architektur (Englisch) |

## Bekannte Einschränkungen

- Die Firmware wurde gebaut, lief aber nie auf den Picos: Die Hardware stand während der Umsetzung nicht mehr
  zur Verfügung. Nicht geprüft sind deshalb die Servobewegung, die Zeitziele (Öffnen ≤ 2 s, Verbuchen ≤ 3 s,
  Antwort < 1 s), WLAN und echte Stromunterbrüche (siehe Testprotokoll).
- Ohne «Durchfahren» (und ohne angeschlossene Schleife) bleibt eine geöffnete Schranke offen.
- Die Kamerasimulation listet alle Parkvorgänge (Testhilfe); in einem echten Parkhaus wäre das nicht öffentlich.
- Die Anmeldung gilt bis zum Abmelden oder Neustart von Pico 2; höchstens 4 Browser gleichzeitig (der älteste
  wird abgemeldet). Alles läuft über unverschlüsseltes HTTP im Test-WLAN.
