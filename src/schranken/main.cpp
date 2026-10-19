#include <Arduino.h>

#include "pins.h"

void setup()
{
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(PIN_ENTRY_LOOP, INPUT_PULLUP);
    pinMode(PIN_EXIT_LOOP, INPUT_PULLUP);
}

void loop()
{
}
