#include <Arduino.h>
#include <WiFi.h>
#include <stdlib.h>
#include <string>

#include "codec.h"
#include "format.h"
#include "net.h"
#include "secrets.h"
#include "status_led.h"
#include "storage.h"
#include "testdata.h"

namespace
{
SessionStore sessions;
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

void loadSessions()
{
    std::string json;
    if (!storageRead("sessions", json) || !sessionsFromJson(json, sessions))
    {
        loadDefaultSessions(sessions);
        storageWrite("sessions", sessionsToJson(sessions));
        Serial.println("sessions: defaults written");
    }
}

void report()
{
    Serial.print("[kasse] WLAN ");
    Serial.print(wifiReady() ? WiFi.localIP().toString() : String("not connected"));
    Serial.print(" | time ");
    Serial.print(timeReady() ? formatDateTime(nowUtc()).c_str() : "not synced");
    Serial.print(" | boot #");
    Serial.print(bootCount);
    Serial.print(" | sessions ");
    Serial.println(sessions.all().size());
}
}

void setup()
{
    Serial.begin(115200);
    statusLedBegin();
    if (!storageBegin())
    {
        Serial.println("LittleFS mount failed");
    }
    bootCount = countBoot();
    loadSessions();
    netBegin(IPAddress(KASSE_IP));
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
