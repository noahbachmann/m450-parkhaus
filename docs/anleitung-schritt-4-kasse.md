# Schritt 4 – Pico 1 «Kasse»: Anleitung und Prüfung

Was in Schritt 4 dazukommt und wie man es Schritt für Schritt prüft. Voraussetzung: Schritt 3 läuft
(LED leuchtet dauernd, Uhrzeit synchronisiert), siehe [aufbau-anleitung.md](aufbau-anleitung.md).

## Was neu ist

| Datei | Inhalt |
|---|---|
| `src/kasse/main.cpp` | Webserver auf Port 80, Routen, Speichern der Parkvorgänge nach jeder Änderung |
| `src/kasse/pages.cpp` | HTML der Seiten Kasse und Automat |
| `lib/kasse_view/` | Texte der Seiten (Dauer, Gebühr, Tarifhinweis, Zahlungsmeldungen), Eingabe des Bargeldbetrags |
| `test/native/test_kasse_view/` | Unit-Tests dazu (9 Fälle) |

Adressen auf Pico 1:

| Adresse | Zweck |
|---|---|
| `http://<KASSE_IP>/kasse` | Kasse: Kennzeichen suchen, Einfahrt/Dauer/Gebühr, bezahlen, Liste aller Parkvorgänge, Testdaten zurücksetzen |
| `http://<KASSE_IP>/automat` | Automat: per Kennzeichen bezahlen (Karte, TWINT, Bargeld mit Rückgeld) |
| `GET /api/status?kz=…` | Zahlungsstatus für Pico 2 (TF-29) |
| `POST /api/entry`, `POST /api/exit` | Ein-/Ausfahrt (bis Schritt 5 per curl) |
| `POST /reset` | Testdaten zurücksetzen (keine Parkvorgänge, Transaktionszähler auf 1) |

Der Boot-Zähler aus Schritt 3 ist auf der Kasse entfernt; die Datei `/boot.json` bleibt ungenutzt liegen.

---

## 1. Unit-Tests und Build

```
pio test -e native
pio run
```

- [ ] `pio test -e native`: **10 Suites, 72 Testfälle**, alle `PASSED` (neu: `test_kasse_view`).
- [ ] `pio run`: `kasse` und `schranken` je **SUCCESS**.

## 2. Flashen und Start

```
pio run -e kasse -t upload
pio device monitor -b 115200
```

- [ ] Konsole zeigt `sessions: loaded …` (oder beim ersten Mal `sessions: defaults written`).
- [ ] Nach dem WLAN-Connect: `HTTP server listening on port 80`.
- [ ] LED leuchtet dauernd.

Jeder Aufruf erscheint in der Konsole, z. B. `[http] POST /api/entry -> 201`.

## 3. Terminal vorbereiten

Die Befehle sind für **Git Bash** geschrieben (wird mit Git für Windows installiert). In PowerShell
heisst curl `curl.exe`, und JSON in Anführungszeichen wird dort anders behandelt – darum Git Bash nehmen.

Laptop mit dem Hotspot verbinden, dann die Adresse der Kasse setzen (Wert von `KASSE_IP` aus
`include/secrets.h`, mit Punkten):

```bash
K=http://172.20.10.11
J="Content-Type: application/json"
curl -i "$K/api/status?kz=LU123456"
```

- [ ] Antwort `HTTP/1.1 200 OK` mit `{"kz":"LU123456","status":"unbekannt"}`.

> **Wichtig:** Bei `POST /api/entry` und `/api/exit` immer `-H "$J"` mitgeben. Ohne den Header
> schickt curl das Formular-Format, die Kasse sieht keinen JSON-Body und antwortet `400 bad_request`.

Hilfsfunktion für Einfahrten mit zurückdatierter Einfahrtszeit (Sekunden vor jetzt):

```bash
entry() { curl -s -X POST $K/api/entry -H "$J" -d "{\"kz\":\"$1\",\"entry\":$(( $(date +%s) - $2 ))}"; echo; }
```

Beispiel: `entry "LU 123456" 4800` = Einfahrt vor 1 h 20 min.

## 4. Testfälle

Vor jedem Testfall die Testdaten zurücksetzen – entweder Knopf **«Testdaten zurücksetzen»** unten auf
der Kasse-Seite oder:

```bash
curl -s -X POST $K/reset -o /dev/null
```

### 4.1 TF-18 – Gebühr für R-01 bis R-12

Setzt pro Referenzwert die Daten zurück, legt die Einfahrt für KZ-01 an und liest die Gebühr:

```bash
curl -s -X POST $K/reset -o /dev/null
curl -s -X POST $K/api/entry -H "$J" -d '{"kz":"LU123456"}' > /dev/null
echo "R-01: $(curl -s "$K/api/status?kz=LU123456")"
i=2
for d in 840 960 3540 3660 9000 35940 36060 86340 86460 172740; do
  curl -s -X POST $K/reset -o /dev/null
  entry LU123456 $d > /dev/null
  printf "R-%02d: %s\n" $i "$(curl -s "$K/api/status?kz=LU123456")"
  i=$((i+1))
done
curl -s -X POST $K/reset -o /dev/null
echo "R-12: $(entry LU123456 -3600)"
```

| R | Dauer | erwartet `fee` |
|---|---|---|
| R-01 | sofort | 0 |
| R-02 | 14 min | 0 |
| R-03 | 16 min | 200 |
| R-04 | 59 min | 200 |
| R-05 | 1 h 01 min | 400 |
| R-06 | 2 h 30 min | 600 |
| R-07 | 9 h 59 min | 2000 |
| R-08 | 10 h 01 min | 2000 |
| R-09 | 23 h 59 min | 2000 |
| R-10 | 24 h 01 min | 2200 |
| R-11 | 47 h 59 min | 4000 |
| R-12 | 1 h in der Zukunft | `{"error":"entry_in_future"}`, keine Gebühr |

`fee` ist in Rappen (600 = CHF 6.00).

- [ ] Alle Werte stimmen.
- [ ] Stichprobe in der Kasse: R-11 anlegen, `http://<KASSE_IP>/kasse?kz=LU123456` öffnen →
  Gebühr **«CHF 40.00»** im Format `CHF 0.00`.

### 4.2 TF-19 – Dauer, Betrag, Tarifhinweis

```bash
curl -s -X POST $K/reset -o /dev/null
entry "LU 123456" 4800
```

Kasse öffnen, «LU 123456» suchen.

- [ ] Kennzeichen `LU123456`, Einfahrt als `TT.MM.JJJJ HH:MM:SS` (= jetzt − 80 min).
- [ ] Parkdauer **«1 h 20 min»**, Gebühr **«CHF 4.00»**.
- [ ] Tarif: «2 angefangene Stunden à CHF 2.00 ab Einfahrt (15 Freiminuten überschritten)».
- [ ] Dasselbe im Automaten (`/automat`).

(Die eigene Tarifseite kommt erst in Phase 2.)

### 4.3 TF-20 – Bezahlen an der Kasse

Gleiche Daten wie TF-19 (KZ-01, CHF 4.00).

1. Kasse, «LU 123456» suchen.
2. **«Karte OK»** wählen, **«Bezahlen»**.
3. Nochmals **«Bezahlen»**.

- [ ] Nach 1–2 s grüne Meldung **«Bezahlt – CHF 4.00, Karte – Transaktion TX-000001»**.
- [ ] Zahlungsstatus «bezahlt», Zahlung «Karte, TX-000001».
- [ ] Zweiter Versuch: **«bereits bezahlt»**; in der Liste weiterhin nur TX-000001.
- [ ] Seite neu laden (F5) bucht nichts doppelt.
- [ ] `curl "$K/api/status?kz=LU123456"` → `"status":"bezahlt","fee":400`.

### 4.4 TF-21 – Bezahlen am Automaten

```bash
curl -s -X POST $K/reset -o /dev/null
entry LU123456 9000      # CHF 6.00
entry ZH98765 9000       # CHF 6.00
entry AG1 1800           # CHF 2.00
```

Im Automaten (`/automat`):

1. «LU 123456» → **Bargeld**, Betrag `10.00` → «Bezahlen».
2. «ZH 98765» → **TWINT** → «Bezahlen».
3. «AG 1» → **Bargeld** `1.00` → «Bezahlen»; dann nochmals **Bargeld** `1.00` → «Bezahlen».

- [ ] KZ-01: «Bezahlt – CHF 6.00, Bargeld – … – **Rückgeld CHF 4.00**», innert 3 s.
- [ ] KZ-02: «Bezahlt – CHF 6.00, TWINT – …», innert 3 s.
- [ ] KZ-08 nach 1. Einwurf: **«Restbetrag CHF 1.00 – unbezahlt»**, Zahlungsstatus «unbezahlt»,
  «Bargeld eingeworfen CHF 1.00».
- [ ] KZ-08 nach 2. Einwurf: «Bezahlt – CHF 2.00, Bargeld – … – **Rückgeld CHF 0.00**».
- [ ] Optional: «Karte abgelehnt» → «Zahlung abgelehnt – anderes Zahlungsmittel wählen», Status bleibt «unbezahlt».
- [ ] Optional: Bargeld ohne Betrag oder mit `abc` → «Bitte eingeworfenen Betrag in CHF eingeben», nichts gebucht.

### 4.5 TF-22 – Automat ohne Parkvorgang

Im Automaten «SZ 555» suchen.

- [ ] **«Kein offener Parkvorgang für SZ555»**, kein Zahlungsformular.

### 4.6 TF-29 – Zahlungsstatus per curl (20 Anfragen, je < 1 s)

```bash
curl -s -X POST $K/reset -o /dev/null
entry LU123456 4800 > /dev/null
entry ZH98765 4800 > /dev/null
```

KZ-01 an der Kasse mit «Karte OK» bezahlen, dann:

```bash
for n in 1 2 3 4; do
  for kz in LU123456 ZH98765 SZ555 "" "LU-12%2345"; do
    curl -s -o /tmp/r.json -w "%{http_code} %{time_total}s " "$K/api/status?kz=$kz"; cat /tmp/r.json; echo
  done
done
```

(`%23` ist das URL-codierte `#`, also «LU-12#45».)

- [ ] `LU123456` → 200 `"status":"bezahlt"`.
- [ ] `ZH98765` → 200 `"status":"unbezahlt","fee":400`.
- [ ] `SZ555` → 200 `"status":"unbekannt"`.
- [ ] leer und `LU-12#45` → **400** `{"error":"invalid_plate"}`, kein Absturz (Konsole läuft weiter).
- [ ] Alle 20 Zeiten **< 1 s**.

### 4.7 Ausfahrt per curl (Vorbereitung für Schritt 5)

Mit den Daten aus 4.6:

```bash
curl -s -X POST $K/api/exit -H "$J" -d '{"kz":"ZH98765"}'; echo   # 402 unpaid, fee 400
curl -s -X POST $K/api/exit -H "$J" -d '{"kz":"LU123456"}'; echo  # 200 closed
curl -s -X POST $K/api/exit -H "$J" -d '{"kz":"SZ555"}'; echo     # 404 no_session
```

- [ ] Antworten wie im Kommentar.
- [ ] Kasse, «LU 123456»: Status «abgeschlossen», Ausfahrtszeit gesetzt; nach 1 min Neuladen ändert sich die Dauer nicht mehr.

### 4.8 Speicherung über Neustart (Teil von TF-30)

1. Ein Kennzeichen bezahlen (z. B. wie 4.3).
2. USB-Kabel von Pico 1 ziehen, 5 s warten, wieder einstecken.
3. Warten, bis die LED dauernd leuchtet.

- [ ] Konsole: `sessions: loaded …` (nicht `defaults written`).
- [ ] Kasse zeigt alle Parkvorgänge wie vorher, der bezahlte weiterhin «bezahlt» mit derselben Transaktion.
- [ ] Nächste Zahlung erhält die nächste Nummer (kein zweites TX-000001).

## 5. Fehlerbehebung

| Problem | Ursache / Lösung |
|---|---|
| `400 {"error":"bad_request"}` bei Ein-/Ausfahrt | Header `-H "Content-Type: application/json"` fehlt, oder JSON falsch gequotet (PowerShell → Git Bash nehmen). |
| `503 time_not_synced` | Uhrzeit noch nicht da (LED blinkt). Hotspot braucht Internet. |
| `409 already_open` | KZ hat schon einen Parkvorgang → vorher `POST /reset`. |
| `400 entry_in_future` bei kleinen Werten | Laptop-Uhr geht vor. Bei R-01 darum ohne `entry` senden (macht das Skript so). |
| Seite lädt manchmal einige Sekunden | Der Pico bedient eine Verbindung nach der anderen; Chrome öffnet «auf Vorrat» Verbindungen, die ihn kurz blockieren. Für Zeitmessungen (TF-29) die Browser-Tabs der Kasse schliessen oder Firefox verwenden. |
| `curl: (7) Failed to connect` | Pico nicht im WLAN, falsche IP in `K`, Laptop in einem anderen Netz. |
| Nach Neustart leere Liste | Konsole prüfen: `LittleFS mount failed` oder `sessions: write FAILED` melden. |
