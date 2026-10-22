#include <Arduino.h>

#include "status_led.h"

namespace
{
constexpr unsigned long BLINK_HALF_PERIOD_MS = 250;

// Tracked here because digitalRead() on the Pico W LED (CYW43 pin) does not return the written value.
bool ledOn = false;
unsigned long lastToggleMs = 0;

void setLed(bool on)
{
    if (on != ledOn)
    {
        digitalWrite(LED_BUILTIN, on ? HIGH : LOW);
        ledOn = on;
    }
}
}

void statusLedBegin()
{
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    ledOn = false;
}

void statusLedUpdate(bool ready, unsigned long nowMs)
{
    if (ready)
    {
        setLed(true);
        return;
    }
    if (nowMs - lastToggleMs >= BLINK_HALF_PERIOD_MS)
    {
        lastToggleMs = nowMs;
        setLed(!ledOn);
    }
}
