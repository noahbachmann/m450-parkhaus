# Parkhaus mit Schranke

Projekt im Modul 450 (BBZW Sursee, Klasse sina24cL).

## Gruppenmitglieder

- Noah Bachmann
- Paul Willimann

## Kurzbeschreibung der Umsetzung

Prototyp eines Parkhauses mit Ein- und Ausfahrtsschranke auf zwei Raspberry Pi Pico W,
programmiert in C++ mit dem Arduino-Framework und gebaut mit PlatformIO.

- **Pico 1 (Kasse):** Berechnung der Parkgebühr, Speicherung der Parkvorgänge, Zahlungen;
  JSON-Schnittstelle für Pico 2 (keine eigenen Bedienseiten).
- **Pico 2 (Schranken):** steuert beide Schranken (Servos), Registrierung der Kennzeichen und die
  ganze Weboberfläche: Kamerasimulation (Kennzeichen wird manuell eingegeben), Tarif und – nach der
  Anmeldung als Betriebspersonal – Kasse, Registrierung und Leitstelle. Die Kassen-Daten holt Pico 2
  bei Pico 1.

Bei der Ausfahrt fragt Pico 2 bei Pico 1 nach, ob die Gebühr bezahlt wurde. Nur dann öffnet die Schranke.

## Umgesetzte User Stories / Anforderungen

Phase 1 = Minimalumfang des Auftrags plus Registrierung; Phase 2 = übriger Umfang des Testkonzepts.
«umgesetzt» heisst: implementiert und mit den Unit- und Systemtests auf dem PC geprüft (siehe
[docs/testprotokoll.md](docs/testprotokoll.md)), zusätzlich im PC-Simulator von Hand durchgespielt.
User Stories, die mit dem Pico-Aufbau nicht umsetzbar sind (App, E-Mail, Sensoren, Zahlungsanbieter usw.),
sind nicht aufgeführt.

| ID | Anforderung | Status |
|---|---|---|
| US-01 | Registrierung der Kennzeichen über die Weboberfläche (AK2, AK4: Name + Kennzeichen, keine Duplikate) | umgesetzt |
| US-06 | Einfahrtsschranke öffnet bei registriertem Kennzeichen (AK1, AK2, Normalisierung statt AK3), Terminal zeigt das erkannte Kennzeichen (AK4) | umgesetzt |
| US-07 | Parkuhr startet automatisch bei der Einfahrt, ein offener Parkvorgang pro Kennzeichen (AK1, 2, 4, 5) | umgesetzt |
| US-13 | Erkennung bei der Ausfahrt, Parkvorgang wird beendet (AK1, AK2); ohne Parkvorgang bleibt die Schranke zu (AK3); «Fahrzeug wendet» lässt den Parkvorgang offen (AK4) | umgesetzt |
| US-14 | Parkdauer und Gebühr an der Kasse, angefangene Stunden und Freiminuten (AK1, AK2); gültiger Tarif einsehbar (AK3) | umgesetzt |
| US-16 | Ausfahrt nur bei bezahlter Gebühr oder CHF 0.00 (AK1); abgelehnte Zahlung mit Grund (AK2), Bezahlen direkt am Ausfahrtsterminal (AK3), nach 5 Ablehnungen wird die Leitstelle benachrichtigt (AK4); jede Öffnung ohne Zahlung wird mit Grund und Benutzer protokolliert (AK5) | umgesetzt |
| US-17 | Bezahlen an der Kasse über das Kennzeichen: Karte, TWINT, Bargeld mit Rückgeld (AK1, AK2); einen separaten Automaten gibt es nicht | umgesetzt |
| US-08 | Hilfe bei nicht erkanntem Kennzeichen: Leitstelle sieht Spur, Kennzeichen, Zahlungsstatus; Fernöffnung nur mit Grund, eröffnet bei der Einfahrt einen Parkvorgang, wird protokolliert (AK2–5); bei der Einfahrt auch mit «Neuer Kunde registrieren» | umgesetzt (Phase 2) |
| US-09 | Sichere Schrankenbewegung: schliesst erst, wenn das Fahrzeug durchgefahren ist und die Schleife frei ist, Stopp und Öffnen beim Schliessen, Störmeldung nach 5 min (AK1–3) | umgesetzt (Phase 2) |
| Auftrag | Ein- und Ausfahrtsschranke (Servos) | umgesetzt, auf Hardware ungetestet |
| Auftrag | Berechnung der Parkgebühr | umgesetzt |
| Auftrag | Bezahlung an der Kasse (der Automat ist in der Kasse aufgegangen) | umgesetzt |

### Weboberflächen

Alle Seiten laufen auf Pico 2 und haben eine gemeinsame Navigation. Ohne Anmeldung sieht man nur
Kamerasimulation und Tarif (🔒 «Anmelden»). Nach der Anmeldung als «betrieb» zeigt die Navigation
👤 «betrieb (Admin)» mit «Abmelden» und zusätzlich Kasse, Registrierung und Betrieb.

| Pico | Adresse | Zugang | Inhalt |
|---|---|---|---|
| Pico 2 | `/kamera` (Startseite) | alle | Kamerasimulation: Kennzeichen bei Ein- und Ausfahrt eingeben, optional rückdatierte Einfahrtszeit; «Durchfahren», «Fahrzeug bleibt stehen», Ruftaste, «Fahrzeug wendet», Bezahlen am Ausfahrtsterminal; darunter alle Parkvorgänge (offene zuerst) als Testhilfe |
| Pico 2 | `/tarif` | alle | Gültiger Tarif mit Beispielen |
| Pico 2 | `/login` | alle | Anmeldung Betriebspersonal |
| Pico 2 | `/kasse` | Betrieb | Kennzeichen suchen, Dauer und Gebühr, bezahlen (Karte, TWINT, Bargeld); Liste aller Parkvorgänge, offene zuerst |
| Pico 2 | `/registrierung` | Betrieb | Kennzeichen registrieren und löschen |
| Pico 2 | `/betrieb` | Betrieb | Leitstelle: Spuren, Fernöffnung mit Grund (bei der Einfahrt auch «Neuer Kunde registrieren»), Ereignisprotokoll mit Filter, Testdaten zurücksetzen |
| Pico 1 | `/` | alle | Hinweis mit Link auf Pico 2 |
| Pico 1 | `/api/status`, `/api/sessions`, `/api/entry`, `/api/exit`, `/api/pay`, `/api/reset` | Pico 2 | JSON-Schnittstelle |

Die Schranke schliesst erst, wenn das Fahrzeug durchgefahren ist: Auf der Kamerasimulation erscheint dazu
der Knopf «Durchfahren» (er ersetzt die Sicherungsschleife). Er führt zu «LU123456 durchgefahren – Schranke
schliesst», nach dem Schliessen zu «… – Schranke geschlossen». Ist eine Schleife angeschlossen, zählt auch
das Freiwerden der Schleife als Durchfahrt.

«Fahrzeug bleibt stehen» belegt die Sicherungsschleife von der Webseite aus (Anzeige «Sicherungsschleife:
belegt»), «Fahrzeug fährt weiter» gibt sie wieder frei und zählt als Durchfahrt. Damit lassen sich TF-12,
TF-13/14 und TF-15 im Simulator von Hand prüfen. Sicherheitsabschaltung (TF-13): Schranke öffnen, nach
mindestens 5 s «Durchfahren» klicken – die Schranke schliesst jetzt 2 s lang – und sofort «Fahrzeug bleibt
stehen» klicken. Die Schranke ist wieder «offen», im Ereignisprotokoll steht «Sicherheitsabschaltung».

Hardware-Aufbau, Pinbelegung und WLAN-Einrichtung: [docs/aufbau-anleitung.md](docs/aufbau-anleitung.md)

## Build und Tests

```
pio run -e kasse        # Firmware Pico 1 (Kasse) bauen
pio run -e schranken    # Firmware Pico 2 (Schranken) bauen
pio test -e native      # Unit- und Systemtests auf dem PC (ohne Board)
pio device monitor -b 115200
```

### PC-Simulator (ohne Hardware)

Beide Picos laufen als zwei Programme auf dem PC, mit derselben Logik aus `lib/` wie die Firmware.
Wie auf der Hardware hält **Pico 1 «Kasse»** die Parkvorgänge und die Logik; **Pico 2 «Schranken»**
fragt die Kasse per HTTP ab (Timeout 1000 ms).

**Mit Docker (empfohlen):**

```
docker compose up --build
```

**Ohne Docker** (zwei Terminals, PlatformIO):

```
pio run -e sim_kasse -t exec
pio run -e sim_schranken -t exec
```

Danach im Browser:

| Adresse | Inhalt |
|---|---|
| `http://localhost:8082` | Pico 2: alle Seiten (Kamerasimulation, Tarif; Anmeldung «betrieb» / «parkhaus» für Kasse, Registrierung, Betrieb) |
| `http://localhost:8081/api/status?kz=LU123456` | Pico 1: JSON-Schnittstelle (auch `GET /api/sessions`, `POST /api/entry`, `/api/exit`, `/api/pay`, `/api/reset`) |

- Pico 1 «ausschalten» (TF-11, TF-30): `docker compose stop kasse` bzw. das Kasse-Programm beenden;
  wieder einschalten mit `docker compose start kasse`. Die Schranken bleiben dann zu, Kasse und Liste der
  Parkvorgänge zeigen «Kasse nicht erreichbar».
- Die Schranken werden als Text angezeigt («offen», «schliesst», «geschlossen»); Seite neu laden, um
  den aktuellen Stand zu sehen. Die Konsole zeigt alle Anfragen, Entscheide und Schrankenbewegungen.
- Daten liegen in `sim-data/kasse/` und `sim-data/schranken/` (gleiches JSON-Format wie im Flash der
  Picos). Ordner löschen = Testdaten.
- Die Uhr ist die Uhr des PCs.

### Tests

- **Unit-Tests** pro Modul in `test/native/test_<modul>/` (Tarif, Kennzeichen, Parkvorgänge, Zahlung,
  Schnittstelle, Kassen-Client, Anmeldung, Seiten, Schranke, Ein-/Ausfahrtsentscheid).
- **Systemtest** `test/native/test_system/`: die Testfälle TF-01 bis TF-32 des Testkonzepts mit der Logik
  beider Picos in einem Programm.
- Die Tests auf der Hardware (Servos, Zeitmessungen, WLAN, Stromunterbruch) konnten nicht durchgeführt
  werden, weil die Picos ab Schritt 4 nicht mehr zur Verfügung standen. TF-33 (Usability mit einer
  fremden Person) und TF-34 (exploratives Testen) sind manuelle Tests und lassen sich mit dem
  PC-Simulator durchführen. Details: [docs/testprotokoll.md](docs/testprotokoll.md).

## Bekannte Einschränkungen

- Firmware nur gebaut, nie auf den Picos gelaufen (ab Schritt 4 keine Hardware mehr).
- Ohne Klick auf «Durchfahren» (und ohne angeschlossene Schleife) bleibt eine geöffnete Schranke offen.
- Die Anmeldung gilt bis zum Abmelden oder Neustart von Pico 2; höchstens 4 Browser gleichzeitig (der
  älteste wird abgemeldet).
- TF-10 von Hand: zwischen den beiden Einfahrten mindestens 30 s warten. Eine Wiederholung innert 30 s
  gilt als dieselbe Einfahrt (falls die Antwort der Kasse zu spät kam) und öffnet die Schranke.
