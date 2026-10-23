# Parkhaus mit Schranke

Projekt im Modul 450 (BBZW Sursee, Klasse sina24cL).

## Gruppenmitglieder

- Noah Bachmann
- Paul Willimann

## Kurzbeschreibung der Umsetzung

Prototyp eines Parkhauses mit Ein- und Ausfahrtsschranke auf zwei Raspberry Pi Pico W,
programmiert in C++ mit dem Arduino-Framework und gebaut mit PlatformIO.

- **Pico 1 (Kasse):** Weboberfläche für Kasse und Automat, Berechnung der Parkgebühr,
  Speicherung der Parkvorgänge, Schnittstelle für den Zahlungsstatus.
- **Pico 2 (Schranken):** steuert beide Schranken (Servos), Registrierung der Kennzeichen und
  eine Weboberfläche, die die Kennzeichenkamera simuliert (Kennzeichen wird manuell eingegeben).

Bei der Ausfahrt fragt Pico 2 bei Pico 1 nach, ob die Gebühr bezahlt wurde. Nur dann öffnet die Schranke.

## Umgesetzte User Stories / Anforderungen

Phase 1 = Minimalumfang des Auftrags plus Registrierung. «umgesetzt» heisst: implementiert und mit den
Unit- und Systemtests auf dem PC geprüft (siehe [docs/testprotokoll.md](docs/testprotokoll.md)).

| ID | Anforderung | Status |
|---|---|---|
| US-01 | Registrierung der Kennzeichen über die Weboberfläche (AK2, AK4: Name + Kennzeichen, keine Duplikate) | umgesetzt |
| US-06 | Einfahrtsschranke öffnet bei registriertem Kennzeichen (AK1, AK2, Normalisierung statt AK3) | umgesetzt |
| US-07 | Parkuhr startet automatisch bei der Einfahrt, ein offener Parkvorgang pro Kennzeichen (AK1, 2, 4, 5) | umgesetzt |
| US-13 | Erkennung bei der Ausfahrt, Parkvorgang wird beendet (AK1, AK2) | umgesetzt |
| US-14 | Parkdauer und Gebühr an Kasse und Automat, angefangene Stunden und Freiminuten (AK1, AK2) | umgesetzt |
| US-16 | Ausfahrt nur bei bezahlter Gebühr oder CHF 0.00 (AK1) | umgesetzt |
| US-17 | Bezahlen am Automaten über das Kennzeichen: Karte, TWINT, Bargeld mit Rückgeld (AK1, AK2) | umgesetzt |
| US-09 | Sichere Schrankenbewegung: schliesst nur bei freier Schleife, Stopp und Öffnen beim Schliessen, Störmeldung nach 5 min (AK1–3) | umgesetzt (Phase 2) |
| Auftrag | Ein- und Ausfahrtsschranke (Servos) | umgesetzt, auf Hardware ungetestet |
| Auftrag | Berechnung der Parkgebühr | umgesetzt |
| Auftrag | Bezahlung an Kasse oder Automat | umgesetzt |

### Weboberflächen

| Pico | Adresse | Inhalt |
|---|---|---|
| Pico 1 | `/kasse`, `/automat` | Kennzeichen suchen, Dauer und Gebühr, bezahlen; Kasse mit Liste aller Parkvorgänge |
| Pico 1 | `/api/status`, `/api/entry`, `/api/exit` | JSON-Schnittstelle für Pico 2 |
| Pico 2 | `/kamera` | Kamerasimulation: Kennzeichen bei Ein- und Ausfahrt eingeben, optional rückdatierte Einfahrtszeit |
| Pico 2 | `/registrierung` | Kennzeichen registrieren, löschen, Testdaten zurücksetzen |

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
| `http://localhost:8081/kasse`, `/automat` | Pico 1: Kasse und Automat |
| `http://localhost:8081/api/status?kz=LU123456` | Pico 1: JSON-Schnittstelle (auch `POST /api/entry`, `/api/exit`) |
| `http://localhost:8082/kamera`, `/registrierung` | Pico 2: Kamerasimulation und Registrierung |
| `http://localhost:8082/sim` | Steuerung: Sicherungsschleifen belegen/freigeben |

- Auf jeder Seite verlinkt eine dunkle Leiste alle Seiten beider «Picos».
- Pico 1 «ausschalten» (TF-11, TF-30): `docker compose stop kasse` bzw. das Kasse-Programm beenden;
  wieder einschalten mit `docker compose start kasse`. Die Schranken bleiben dann zu.
- Die Schranken werden als Text angezeigt («offen», «schliesst», «geschlossen»); Seite neu laden, um
  den aktuellen Stand zu sehen. Die Konsole zeigt alle Anfragen, Entscheide und Schrankenbewegungen.
- Daten liegen in `sim-data/kasse/` und `sim-data/schranken/` (gleiches JSON-Format wie im Flash der
  Picos). Ordner löschen = Testdaten.
- Die Uhr ist die Uhr des PCs.

### Tests

- **Unit-Tests** pro Modul in `test/native/test_<modul>/` (Tarif, Kennzeichen, Parkvorgänge, Zahlung,
  Schnittstelle, Seiten, Schranke, Ein-/Ausfahrtsentscheid).
- **Systemtest** `test/native/test_system/`: die Phase-1-Testfälle des Testkonzepts (TF-01 bis TF-32) mit
  der Logik beider Picos in einem Programm.
- Die Tests auf der Hardware (Servos, Zeitmessungen, WLAN, Stromunterbruch) konnten nicht durchgeführt
  werden, weil die Picos ab Schritt 4 nicht mehr zur Verfügung standen. Details:
  [docs/testprotokoll.md](docs/testprotokoll.md).
