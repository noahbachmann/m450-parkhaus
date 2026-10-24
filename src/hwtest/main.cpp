#include <Arduino.h>
#include <Servo.h>

#include "pins.h"

// Wiring check for Pico 2 without the real firmware: both servos open (90°) and close (0°) every 2 s, the
// console shows the safety loops. Flash with: pio run -e hwtest -t upload

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
