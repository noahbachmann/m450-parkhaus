#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "net.h"
#include "secrets.h"

namespace
{
constexpr time_t MIN_VALID_EPOCH = 1700000000; // before NTP sync the clock starts near 1970
constexpr unsigned long RECONNECT_INTERVAL_MS = 20000;

IPAddress ownIp;
bool ntpStarted = false;
bool wasConnected = false;
bool wasTimeValid = false;
unsigned long lastAttemptMs = 0;

void startWifi()
{
    // Argument order of this core: IP, DNS, gateway, subnet. The hotspot is DNS and gateway.
    WiFi.config(ownIp, IPAddress(GATEWAY_IP), IPAddress(GATEWAY_IP), IPAddress(SUBNET_MASK));
    WiFi.beginNoBlock(WIFI_SSID, WIFI_PASSWORD);
}
}

void netBegin(const IPAddress& ip)
{
    ownIp = ip;
    startWifi();
    lastAttemptMs = millis();
}

void netUpdate(unsigned long nowMs)
{
    bool connected = wifiReady();
    if (connected != wasConnected)
    {
        wasConnected = connected;
        Serial.println(connected ? "WLAN connected" : "WLAN lost");
    }

    if (!connected)
    {
        if (nowMs - lastAttemptMs >= RECONNECT_INTERVAL_MS)
        {
            lastAttemptMs = nowMs;
            Serial.println("WLAN: retrying");
            WiFi.disconnect();
            startWifi();
        }
        return;
    }
    lastAttemptMs = nowMs;

    if (!ntpStarted)
    {
        NTP.begin("pool.ntp.org", "time.google.com"); // keeps resyncing in the background
        ntpStarted = true;
    }

    bool valid = timeReady();
    if (valid != wasTimeValid)
    {
        wasTimeValid = valid;
        Serial.println(valid ? "NTP synced" : "NTP lost");
    }
}

bool wifiReady()
{
    return WiFi.status() == WL_CONNECTED;
}

bool timeReady()
{
    return time(nullptr) > MIN_VALID_EPOCH;
}

int64_t nowUtc()
{
    return static_cast<int64_t>(time(nullptr));
}
