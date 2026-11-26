#include <Arduino.h>
#include <Servo.h>

#include "barrier.h"
#include "pins.h"

// Wiring check for Pico 2 without the real firmware: both servos open and close every TOGGLE_INTERVAL_MS, the
// console shows the safety loops. Flash with: pio run -e hwtest -t upload

namespace
{
constexpr uint32_t TOGGLE_INTERVAL_MS = 2000;
}

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
    int angle = open ? BARRIER_OPEN_ANGLE : BARRIER_CLOSED_ANGLE;
    entryServo.write(angle);
    exitServo.write(angle);

    bool entryPressed = digitalRead(PIN_ENTRY_LOOP) == LOW;
    bool exitPressed = digitalRead(PIN_EXIT_LOOP) == LOW;
    Serial.printf("Schranken: %s  Schleife Einfahrt: %s  Ausfahrt: %s\n", open ? "offen" : "zu",
                  entryPressed ? "belegt" : "frei", exitPressed ? "belegt" : "frei");
    delay(TOGGLE_INTERVAL_MS);
}
