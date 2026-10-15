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

| ID | Anforderung | Status |
|---|---|---|
| US-01 | Registrierung der Kennzeichen über die Weboberfläche | offen |
| US-06 | Einfahrtsschranke öffnet bei registriertem Kennzeichen | offen |
| US-07 | Parkuhr startet automatisch bei der Einfahrt | offen |
| US-13 | Erkennung bei der Ausfahrt, Parkvorgang wird beendet | offen |
| US-14 | Parkdauer und Gebühr nachvollziehen | offen |
| US-16 | Ausfahrt nur bei bezahlter Gebühr | offen |
| US-17 | Bezahlen am Automaten über das Kennzeichen | offen |
| Auftrag | Berechnung der Parkgebühr | offen |
| Auftrag | Bezahlung an Kasse oder Automat | offen |

## Build und Tests

```
pio run                 # Firmware bauen
pio test -e native      # Unit-Tests auf dem PC (ohne Board)
pio device monitor -b 115200
```
