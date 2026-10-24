# Aufbauanleitung Parkhaus-Prototyp

Schritt-für-Schritt-Anleitung für den Hardware-Aufbau mit **2 × Raspberry Pi Pico W** und
**2 × Servo SG90**, die Einrichtung des WLANs und das erste Flashen.

> **Stand:** Verdrahtung, Pins und WLAN-Konfiguration sind festgelegt. WLAN/NTP (Schritt 3) und die
> Kasse-Firmware auf Pico 1 (Schritt 4, Test: [anleitung-schritt-4-kasse.md](anleitung-schritt-4-kasse.md))
> und die Schranken-Firmware auf Pico 2 (Schritt 5) sind umgesetzt. Ab Schritt 4 wurde nur noch mit den
> Unit-Tests auf dem PC geprüft (keine Picos mehr verfügbar); Abschnitt 8 ist auf Hardware ungetestet.
> Mit dem Schnelltest in [Abschnitt 7.3](#73-hardware-schnelltest-optional) lässt sich die Verdrahtung
> aber schon jetzt prüfen.

---

## Inhalt

1. [Überblick](#1-überblick)
2. [Materialliste](#2-materialliste)
3. [Pinbelegung](#3-pinbelegung)
4. [Verdrahtung Schritt für Schritt](#4-verdrahtung-schritt-für-schritt)
5. [Software installieren](#5-software-installieren)
6. [WLAN einrichten](#6-wlan-einrichten)
7. [Flashen](#7-flashen)
8. [Inbetriebnahme und Test](#8-inbetriebnahme-und-test)
9. [Fehlerbehebung](#9-fehlerbehebung)

---

## 1. Überblick

Die beiden Picos haben getrennte Aufgaben und sprechen über das WLAN per HTTP miteinander.

```
              Handy-Hotspot (2.4 GHz, mit Internet für die Uhrzeit)
                 ▲                                   ▲
                 │ WLAN                              │ WLAN
   ┌─────────────┴───────────┐  HTTP (Port 80) ┌─────┴──────────────────────┐
   │  Pico 1  «Kasse»        │ ◄────────────── │  Pico 2  «Schranken»        │
   │  192.168.1.201          │  Zahlungsstatus │  192.168.1.202              │
   │                         │  Ein-/Ausfahrt  │                             │
   │  • Kasse (Schnittstelle)│                 │  • Einfahrtsschranke (Servo)│
   │  • Gebührenberechnung   │                 │  • Ausfahrtsschranke (Servo)│
   │  • Parkvorgänge         │                 │  • 2 Sicherheitsschleifen   │
   │                         │                 │  • alle Webseiten (Kamera-  │
   │  keine externe Hardware │                 │    Simulation, Kasse, …)    │
   └─────────────────────────┘                 └─────────────────────────────┘
```

**Wichtig:** Nur **Pico 2 «Schranken»** wird verdrahtet. Pico 1 «Kasse» braucht nur ein USB-Kabel.

---

## 2. Materialliste

| Anzahl | Teil | Bemerkung |
|---|---|---|
| 2 | Raspberry Pi Pico **W** | mit eingelöteten Stiftleisten; ohne «W» gibt es kein WLAN |
| 2 | Servo SG90 | Einfahrt und Ausfahrt |
| 1 | Breadboard | gross genug für Pico + Servos |
| ca. 10 | Jumper-Kabel | Male-Male; je ein Kabel in jedes Loch des Servosteckers |
| 2 | Micro-USB-Kabel | **Datenkabel**, kein reines Ladekabel |
| 1 | Handy mit Hotspot | oder ein anderer 2.4-GHz-Router |
| optional: 2 | Taster | simulieren die Sicherheitsschleifen – das Programm läuft auch ohne (siehe Schritt 5) |
| optional | Elko 470 µF / 10 V | nur falls der Pico beim Bewegen der Servos neu startet |

---

## 3. Pinbelegung

### 3.1 Pico 2 «Schranken»

| Funktion | GPIO | Physischer Pin | Kabel / Bauteil |
|---|---|---|---|
| Servo Einfahrt – Signal | **GP27** | **32** | Servo 1, orange |
| Servo Einfahrt – Masse | GND | **33** | Servo 1, braun |
| Servo Ausfahrt – Signal | **GP17** | **22** | Servo 2, orange |
| Servo Ausfahrt – Masse | GND | **23** | Servo 2, braun |
| Sicherheitsschleife Einfahrt | **GP14** | **19** | Taster 1 *(optional)* |
| Sicherheitsschleife Ausfahrt | **GP15** | **20** | Taster 2 *(optional)* |
| Masse Taster | GND | **18** | anderes Bein beider Taster *(optional)* |
| Servo-Versorgung 5 V | VBUS | **40** | über die **+**-Schiene an beide Servos, rot |

Alle GND-Pins des Pico sind intern verbunden. Jeder Servo nimmt einfach den GND-Pin direkt neben
seinem Signal-Pin – eine **−**-Schiene braucht es nicht.

> Die Pins sind im Code **nur an einer Stelle** definiert: [`include/pins.h`](../include/pins.h).
> Wer anders verdrahtet, ändert diese Datei **und** die Tabelle in
> [`docs/context/architecture.md`](context/architecture.md).

### 3.2 Pins am Pico finden

Pico von oben betrachtet, **USB-Buchse oben**. Pin 1 ist oben links, Pin 40 oben rechts.
Links wird nach unten gezählt (1–20), rechts von unten nach oben (21–40).

```
                      ┌──[ USB ]──┐
             GP0  1 ──┤●          ├── 40  VBUS  ◄── Servo + (5 V)
             GP1  2 ──┤           ├── 39  VSYS
             GND  3 ──┤           ├── 38  GND
             GP2  4 ──┤           ├── 37  3V3_EN
             GP3  5 ──┤           ├── 36  3V3 OUT   (NICHT für Servos!)
             GP4  6 ──┤           ├── 35  ADC_VREF
             GP5  7 ──┤           ├── 34  GP28
             GND  8 ──┤           ├── 33  GND   ◄── Servo 1 braun
             GP6  9 ──┤           ├── 32  GP27  ◄── Servo 1 orange (Einfahrt)
             GP7 10 ──┤           ├── 31  GP26
             GP8 11 ──┤           ├── 30  RUN
             GP9 12 ──┤           ├── 29  GP22
             GND 13 ──┤           ├── 28  GND
            GP10 14 ──┤           ├── 27  GP21
            GP11 15 ──┤           ├── 26  GP20
            GP12 16 ──┤           ├── 25  GP19
            GP13 17 ──┤           ├── 24  GP18
Taster GND ► GND 18 ──┤           ├── 23  GND   ◄── Servo 2 braun
Taster 1 ►  GP14 19 ──┤           ├── 22  GP17  ◄── Servo 2 orange (Ausfahrt)
Taster 2 ►  GP15 20 ──┤           ├── 21  GP16
                      └───────────┘
```

---

## 4. Verdrahtung Schritt für Schritt

> ⚠️ **Immer ohne USB-Kabel verdrahten.** Erst einstecken, wenn alles kontrolliert ist.

### Schritt 1 – Pico aufs Breadboard

- [ ] Pico 2 «Schranken» mittig über den Graben des Breadboards stecken, USB-Buchse zum Rand.
- [ ] Den Pico mit einem Klebeband als **«2 Schranken»** beschriften (Pico 1 als **«1 Kasse»**).
  Die beiden Boards sehen identisch aus.

### Schritt 2 – 5-V-Schiene

- [ ] **Pin 40 (VBUS)** → rote **+**-Schiene des Breadboards: ein Jumper-Kabel von einem freien Loch
  in der Breadboard-Reihe von Pin 40 auf die **+**-Schiene.

VBUS liefert die 5 V direkt vom USB-Anschluss – genau das, was die SG90 brauchen. Es gibt nur einen
VBUS-Pin; über die Schiene bekommen beide Servos ihre 5 V. Eine **−**-Schiene ist nicht nötig, weil
jeder Servo einen eigenen GND-Pin direkt neben seinem Signal-Pin nutzt.

### Schritt 3 – Servo Einfahrt

Ein SG90 hat drei Adern in einem Stecker. In jedes Loch des Steckers ein Male-Male-Jumper-Kabel
stecken und jedes Kabel in die Breadboard-Reihe des passenden Pins führen:

| Ader | Bedeutung | Anschluss |
|---|---|---|
| braun | GND | **Pin 33 (GND)** |
| rot | 5 V | **+**-Schiene |
| orange | Signal | **Pin 32 (GP27)** |

- [ ] Braun → **Pin 33**
- [ ] Rot → **+**-Schiene
- [ ] Orange → **Pin 32**

### Schritt 4 – Servo Ausfahrt

- [ ] Braun → **Pin 23 (GND)**
- [ ] Rot → **+**-Schiene
- [ ] Orange → **Pin 22 (GP17)**

### Schritt 5 – Sicherheitsschleifen (Taster, optional)

Die Taster simulieren die Sicherheitsschleifen: gedrückt = ein Fahrzeug steht unter der Schranke.
**Sie sind optional – das Programm läuft auch ohne sie.**

| Aufbau | Verhalten der Schranke nach dem Öffnen |
|---|---|
| **ohne Taster** | schliesst automatisch nach der Mindest-Öffnungszeit (5 s) |
| **mit Taster** | bleibt offen, solange der Taster gedrückt ist; schliesst, sobald er losgelassen ist (frühestens nach 5 s) |

Warum das ohne Taster funktioniert: Der Pico schaltet an GP14/GP15 intern einen Pull-up ein. Ist nichts
angeschlossen, liest der Pin HIGH = «Schleife frei». Darum braucht es auch mit Tastern **keinen
Widerstand**: Gedrückt zieht der Taster den Pin auf GND (= belegt).

Falls Taster vorhanden sind:

- [ ] Taster 1: ein Bein → **Pin 19 (GP14)**, anderes Bein → **Pin 18 (GND)**
- [ ] Taster 2: ein Bein → **Pin 20 (GP15)**, anderes Bein → **Pin 18 (GND)**

> Pin 18 liegt direkt neben Pin 19 und 20; beide Taster teilen sich seine Breadboard-Reihe.
> Bei 4-beinigen Tastern die **diagonal** gegenüberliegenden Beine verwenden, dann stimmt es sicher.

### Schritt 6 – Gesamtbild

```
                 Breadboard
   + Schiene ════════════════════════════════════  (rot, 5 V von Pin 40)
                  │rot             │rot
             ┌────┴────┐      ┌────┴────┐      ┌─────┐   ┌─────┐
             │ Servo 1 │      │ Servo 2 │      │ T 1 │   │ T 2 │  ← Taster optional
             │Einfahrt │      │Ausfahrt │      └┬───┬┘   └┬───┬┘
             └─┬─────┬─┘      └─┬─────┬─┘       │   │     │   │
               │     │          │     │         │   └──┬──┘   │
               ▼     ▼          ▼     ▼         ▼      ▼      ▼
   Pin         32    33         22    23        19     18     20
              GP27  GND        GP17  GND       GP14   GND    GP15
             orange braun     orange braun
```

### Schritt 7 – Kontrolle vor dem Einstecken

- [ ] Kein Kabel steckt auf **Pin 36 (3V3 OUT)** – die Servos würden den 3.3-V-Regler überlasten.
  Pin 36 liegt nur vier Pins über dem Servo-1-Signal, also genau hinschauen.
- [ ] Beide roten Servo-Adern stecken auf der **+**-Schiene, die mit Pin 40 verbunden ist.
- [ ] Rot und braun sind nicht vertauscht.
- [ ] Servo 1: orange auf **Pin 32**, braun auf **Pin 33** – die beiden liegen direkt nebeneinander,
  nicht vertauschen. Servo 2 ebenso: orange auf **Pin 22**, braun auf **Pin 23**.
- [ ] Die braunen Adern brauchen keine gemeinsame Schiene: alle GND-Pins des Pico sind intern verbunden.

### Hinweise zur Stromversorgung

- Das 3.3-V-Signal des Pico reicht für den SG90, ein Pegelwandler ist nicht nötig.
- Zwei SG90 an USB-VBUS funktionieren für den Prototyp. Ein Servo zieht beim Anlaufen aber kurz bis
  zu ~650 mA. Startet der Pico beim Bewegen neu oder zittern die Servos:
  1. anderen USB-Port / ein USB-Netzteil statt Laptop-Port verwenden,
  2. einen **470-µF-Elko** zwischen die **+**-Schiene und die Breadboard-Reihe von **Pin 38 (GND)**
     stecken (Minus-Streifen an GND),
  3. oder die Servos aus einem separaten 5-V-Netzteil speisen – **GND des Netzteils mit GND des Pico
     verbinden**, sonst funktioniert das Signal nicht.

---

## 5. Software installieren

1. **VS Code** installieren, dann die Erweiterung **PlatformIO IDE**.
2. Repository klonen und den Ordner `parkhaus` in VS Code öffnen.
3. **Nur unter Windows**, einmalig in einer Admin-Konsole (der Pico-Toolchain-Pfad ist sehr lang):
   ```
   git config --system core.longpaths true
   ```
4. Erster Build (lädt beim ersten Mal den Pico-Core und die Toolchain herunter, das dauert einige Minuten):
   ```
   pio run
   ```
   Am Ende muss für `kasse` und `schranken` je **SUCCESS** stehen.

Verwendet werden: PlatformIO mit dem **earlephilhower Arduino-Core** (`board = rpipicow`).
WLAN, Webserver, HTTP-Client, LittleFS und Servo sind im Core schon enthalten. Zusätzlich wird nur
ArduinoJson geladen (steht in `platformio.ini`).

---

## 6. WLAN einrichten

### 6.1 Hotspot vorbereiten

Beide Picos verbinden sich mit einem **separaten Test-WLAN** – am einfachsten ein Handy-Hotspot.

- [ ] **2.4 GHz verwenden.** Der Pico W kann **kein 5 GHz**.
  - iPhone: *Einstellungen → Persönlicher Hotspot →* **«Maximale Kompatibilität»** einschalten.
  - Android: *Hotspot → Erweitert → AP-Band* auf **2.4 GHz** stellen.
- [ ] Der Hotspot braucht **Internet** (mobile Daten an). Die Picos holen damit beim Start die
  Uhrzeit per NTP. Ohne Uhrzeit bleiben Ein- und Ausfahrt gesperrt.
- [ ] SSID und Passwort ohne Sonderzeichen wie `"` oder `\` wählen.

### 6.2 Subnetz des Hotspots herausfinden

Die Picos bekommen **feste IP-Adressen**, damit Pico 2 immer weiss, wo Pico 1 ist. Diese müssen im
Adressbereich des Hotspots liegen.

1. Laptop mit dem Hotspot verbinden.
2. IP-Adresse des Laptops nachschauen:
   - Windows: `ipconfig` → «IPv4-Adresse» und «Standardgateway»
   - macOS / Linux: `ifconfig` bzw. `ip addr`
3. Typische Werte:

| Hotspot | Gateway | Vorschlag Kasse | Vorschlag Schranken |
|---|---|---|---|
| Android (oft) | 192.168.43.1 | 192.168.43.201 | 192.168.43.202 |
| iPhone | 172.20.10.1 | 172.20.10.11 | 172.20.10.12 |
| Router zu Hause | 192.168.1.1 | 192.168.1.201 | 192.168.1.202 |

> Beim **iPhone** ist das Netz sehr klein (Subnetzmaske `255.255.255.240`, nur Adressen
> 172.20.10.2–14). Darum dort .11 und .12 verwenden und die Maske anpassen.

### 6.3 `secrets.h` anlegen

Die Zugangsdaten stehen in `include/secrets.h`. Diese Datei ist **gitignored** und wird nie committet.

1. Vorlage kopieren:
   ```
   copy include\secrets.example.h include\secrets.h      (Windows)
   cp include/secrets.example.h include/secrets.h        (macOS / Linux)
   ```
2. `include/secrets.h` ausfüllen. Die IPs werden **mit Kommas** geschrieben, nicht mit Punkten:
   ```cpp
   #pragma once

   #define WIFI_SSID "Parkhaus-Test"
   #define WIFI_PASSWORD "geheim123"

   #define KASSE_IP 192, 168, 43, 201
   #define SCHRANKEN_IP 192, 168, 43, 202
   #define GATEWAY_IP 192, 168, 43, 1
   #define SUBNET_MASK 255, 255, 255, 0
   ```
3. Kontrolle: `git status` darf `include/secrets.h` **nicht** anzeigen.

Beide Picos verwenden **dieselbe** `secrets.h`. Welche IP ein Pico nimmt, entscheidet die Firmware
(`kasse` → `KASSE_IP`, `schranken` → `SCHRANKEN_IP`).

### 6.4 So baut die Firmware die Verbindung auf

Zur Orientierung – so baut die Firmware die WLAN-Verbindung mit dem earlephilhower-Core auf.

Der echte Code liegt in `src/common/net.cpp` (WLAN + NTP) und `src/common/status_led.cpp` (LED).
Kurzfassung:

```cpp
IPAddress ip(KASSE_IP);                        // auf Pico 2: SCHRANKEN_IP
WiFi.config(ip, IPAddress(GATEWAY_IP), IPAddress(GATEWAY_IP), IPAddress(SUBNET_MASK)); // IP, DNS, Gateway, Maske
WiFi.beginNoBlock(WIFI_SSID, WIFI_PASSWORD);   // kehrt sofort zurück, Verbindung läuft im Hintergrund

// in loop(): sobald verbunden, Zeitserver per DNS suchen und NTP.begin(server) – bis die Zeit stimmt alle 15 s erneut
// LED blinkt, bis WLAN und Uhrzeit bereit sind, danach Dauerlicht
```

Wichtig für die LED des Pico W: Sie hängt am WLAN-Chip. `digitalRead(LED_BUILTIN)` liefert dort nicht den
zuletzt geschriebenen Wert, darum merkt sich die Firmware den Zustand selbst. Ausserdem darf nichts
blockieren (`WiFi.begin()` wartet bis zu 30 s, `NTP.waitSet()` bis zu 10 s), sonst blinkt die LED nicht.

---

## 7. Flashen

### 7.1 Erstes Flashen (pro Pico einmal)

Ein neuer Pico muss beim ersten Mal im **BOOTSEL-Modus** angeschlossen werden:

1. Weisse Taste **BOOTSEL** auf dem Pico gedrückt halten.
2. USB-Kabel einstecken, dann loslassen.
3. Der Pico erscheint als Laufwerk **RPI-RP2**.
4. Hochladen:
   ```
   pio run -e kasse -t upload        ← Pico «1 Kasse» angeschlossen
   pio run -e schranken -t upload    ← Pico «2 Schranken» angeschlossen
   ```

Ab dem zweiten Mal genügt der Upload-Befehl ohne BOOTSEL – der Pico wird automatisch neu gestartet.

> **Nur einen Pico gleichzeitig anschliessen**, solange man flasht. Sonst landet die Firmware leicht
> auf dem falschen Board.

### 7.2 Serielle Konsole

```
pio device monitor -b 115200
```

Hier erscheinen Statusmeldungen (z. B. die IP-Adresse). Beenden mit `Ctrl + C`.

### 7.3 Hardware-Schnelltest (optional)

Damit lässt sich die Verdrahtung prüfen, bevor die richtige Firmware fertig ist. Den Inhalt
**vorübergehend** in `src/schranken/main.cpp` einfügen und auf Pico 2 flashen:

```cpp
#include <Arduino.h>
#include <Servo.h>

#include "pins.h"

Servo entryServo;
Servo exitServo;

void setup()
{
    Serial.begin(115200);
    pinMode(PIN_ENTRY_LOOP, INPUT_PULLUP);
    pinMode(PIN_EXIT_LOOP, INPUT_PULLUP);
    entryServo.attach(PIN_ENTRY_SERVO);
    exitServo.attach(PIN_EXIT_SERVO);
}

void loop()
{
    // Beide Schranken alle 2 s auf (90°) und zu (0°); Taster werden nur angezeigt
    static bool open = false;
    open = !open;
    entryServo.write(open ? 90 : 0);
    exitServo.write(open ? 90 : 0);

    bool entryPressed = digitalRead(PIN_ENTRY_LOOP) == LOW;
    bool exitPressed = digitalRead(PIN_EXIT_LOOP) == LOW;
    Serial.printf("Schranken: %s  Schleife Einfahrt: %s  Ausfahrt: %s\n", open ? "offen" : "zu",
                  entryPressed ? "belegt" : "frei", exitPressed ? "belegt" : "frei");
    delay(2000);
}
```

Erwartung: Beide Servos fahren alle 2 s auf und zu. Ohne Taster zeigt die Konsole immer «frei»; mit
Tastern wechselt die Anzeige beim Drücken auf «belegt».
Danach die Originaldatei zurückholen:

```
git checkout src/schranken/main.cpp
```

---

## 8. Inbetriebnahme und Test

> Stand nach Roadmap-Schritt 5. Auf Hardware nicht getestet.

### 8.1 Startreihenfolge

1. Handy-Hotspot einschalten (2.4 GHz, mobile Daten an).
2. **Pico 1 «Kasse»** per USB mit Strom versorgen.
3. **Pico 2 «Schranken»** per USB mit Strom versorgen.
4. Auf beide Status-LEDs warten:

| LED | Bedeutung |
|---|---|
| blinkt | noch nicht bereit: WLAN nicht verbunden oder Uhrzeit (NTP) fehlt |
| leuchtet dauernd | bereit |

### 8.2 Verbindung prüfen

Laptop mit demselben Hotspot verbinden, dann:

```
ping 192.168.43.201          ← Kasse
ping 192.168.43.202          ← Schranken
```

Weboberflächen im Browser:

| Adresse | Inhalt |
|---|---|
| `http://<SCHRANKEN_IP>/kamera` | Kamera-Simulation (Einfahrt mit optionaler Einfahrtszeit, Ausfahrt, «Durchfahren», «Fahrzeug bleibt stehen» – wirkt wie ein gedrückter Taster) |
| `http://<SCHRANKEN_IP>/login` | Anmeldung «betrieb»; danach Kasse, Registrierung und Betrieb in der Navigation |
| `http://<KASSE_IP>/` | nur ein Hinweis mit Link auf Pico 2 (Pico 1 hat keine eigenen Bedienseiten) |

Schnittstelle der Kasse direkt testen:

```
curl "http://192.168.43.201/api/status?kz=LU123456"
```

Antwort zum Beispiel: `{"kz":"LU123456","status":"unbekannt"}` – die Kasse ist erreichbar.

### 8.3 Ablauf einmal durchspielen

- [ ] Auf Pico 2 ein Kennzeichen registrieren (z. B. `LU 123456`).
- [ ] In der Kamera-Simulation bei der **Einfahrt** eingeben → Servo 1 öffnet.
- [ ] Schranke schliesst wieder: nach «Durchfahren» auf der Kamera-Simulation oder, mit Taster, nach
  Drücken und Loslassen von Taster 1 (frühestens 5 s nach dem Öffnen).
- [ ] **Ausfahrt** ohne Bezahlung → Servo 2 bleibt zu, Meldung zur offenen Gebühr.
- [ ] Anmelden und an der Kasse (`/kasse` auf Pico 2) bezahlen.
- [ ] **Ausfahrt** erneut → Servo 2 öffnet.

---

## 9. Fehlerbehebung

| Problem | Ursache / Lösung |
|---|---|
| Pico wird am PC nicht erkannt | Reines Ladekabel → Datenkabel verwenden. BOOTSEL beim Einstecken gedrückt halten. |
| Upload unter Windows schlägt fehl | Pico im BOOTSEL-Modus neu anschliessen. Falls «RP2 Boot» ohne Treiber erscheint: mit **Zadig** den WinUSB-Treiber dafür installieren. |
| Build bricht mit Pfad-Fehlern ab (Windows) | `git config --system core.longpaths true` (Abschnitt 5). |
| `secrets.h: No such file` | `include/secrets.example.h` nach `include/secrets.h` kopieren (Abschnitt 6.3). |
| LED blinkt dauernd | Hotspot auf 5 GHz → auf 2.4 GHz stellen. Passwort falsch. Hotspot ohne Internet → NTP schlägt fehl. Serielle Konsole zeigt mehr. |
| Ping geht nicht | IPs in `secrets.h` passen nicht zum Subnetz des Hotspots (Abschnitt 6.2). Laptop im falschen WLAN. |
| «Kasse nicht erreichbar – bitte Ruftaste drücken» | Pico 1 aus, noch nicht bereit (LED blinkt) oder falsche `KASSE_IP`. |
| Servo zittert / Pico startet beim Bewegen neu | Zu wenig Strom → siehe [Hinweise zur Stromversorgung](#hinweise-zur-stromversorgung). |
| Servo bewegt sich gar nicht | Rot/braun vertauscht, Signal auf falschem Pin, keine gemeinsame Masse. |
| Taster wird immer als «belegt» gelesen | Taster falsch herum gedreht (4-Bein-Taster: diagonale Beine nehmen). Ohne Taster: Pin 19/20 darf keine Verbindung zu GND haben. |
| Schranke schliesst nicht | Taster (Sicherheitsschleife) ist noch gedrückt bzw. Pin 19/20 hat Kontakt zu GND – Schranke schliesst erst, wenn die Schleife frei ist. |
