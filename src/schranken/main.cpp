#include <Arduino.h>
#include <WiFi.h>
#include <stdlib.h>
#include <string>

#include "codec.h"
#include "format.h"
#include "net.h"
#include "pins.h"
#include "secrets.h"
#include "status_led.h"
#include "storage.h"
#include "testdata.h"

namespace
{
RegistrationList registrations;
uint32_t bootCount = 0;
unsigned long lastReportMs = 0;

// Step-3 smoke test: proves that a file survives a reboot.
uint32_t countBoot()
{
    std::string text;
    uint32_t boots = 0;
    if (storageRead("boot", text))
    {
        boots = strtoul(text.c_str(), nullptr, 10);
    }
    boots++;
    storageWrite("boot", std::to_string(boots));
    return boots;
}

void loadRegistrations()
{
    std::string json;
    if (!storageRead("registrations", json) || !registrationsFromJson(json, registrations))
    {
        loadDefaultRegistrations(registrations);
        storageWrite("registrations", registrationsToJson(registrations));
        Serial.println("registrations: defaults written");
    }
}

void report()
{
    Serial.print("[schranken] WLAN ");
    Serial.print(wifiReady() ? WiFi.localIP().toString() : String("not connected"));
    Serial.print(" | time ");
    Serial.print(timeReady() ? formatDateTime(nowUtc()).c_str() : "not synced");
    Serial.print(" | boot #");
    Serial.print(bootCount);
    Serial.print(" | registrations ");
    Serial.println(registrations.all().size());
}
}

void setup()
{
    Serial.begin(115200);
    statusLedBegin();
    pinMode(PIN_ENTRY_LOOP, INPUT_PULLUP);
    pinMode(PIN_EXIT_LOOP, INPUT_PULLUP);
    if (!storageBegin())
    {
        Serial.println("LittleFS mount failed");
    }
    bootCount = countBoot();
    loadRegistrations();
    netBegin(IPAddress(SCHRANKEN_IP));
}

void loop()
{
    unsigned long nowMs = millis();
    netUpdate(nowMs);
    statusLedUpdate(wifiReady() && timeReady(), nowMs);
    if (nowMs - lastReportMs >= 5000)
    {
        lastReportMs = nowMs;
        report();
    }
}
