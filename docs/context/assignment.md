# Assignment — "Implementierung Parkhaus mit Schranke"

Source: `aufgabe-umsetzung-parksystem.pdf` (3 pages).

- Duration 180 min, group size 1–2, submission as a **git repository**.
- Task: implement the parking system defined in the earlier task "Anforderungen suchen und definieren"
  (= our user stories, see [requirements.md](requirements.md)).
- Hardware: **two Raspberry Pi Pico**, programmed with **Arduino**.
- **Minimal scope:** entry and exit barrier, **fee calculation**, **payment of the fee**.

## Deliverable

Repository with all source files and a `README.md` (German) containing:
- Gruppenmitglieder (group members)
- Kurze Beschreibung der Umsetzung (short description of the implementation)
- Auflistung der umgesetzten User Stories / Anforderungen (list of implemented stories/requirements)

## Hardware hints from the PDF

- Pico W pinout shown (GP0..GP28, GND pins 3/8/13/18/23/28/33/38, VBUS pin 40, VSYS 39, 3V3 OUT 36).
- Servo **SG90**: 3 wires — orange = signal, red = VCC, brown = GND.
- Example wiring (breadboard):

| SG90 wire | Pico pin |
|---|---|
| red (VCC) | VBUS, 5 V (pin 40) |
| brown (GND) | GND (pin 8) |
| orange (signal) | **GP8** (pin 11) |
