#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "net.h"
#include "secrets.h"

namespace
{
constexpr time_t MIN_VALID_EPOCH = 1700000000; // before NTP sync the clock starts near 1970
constexpr unsigned long RECONNECT_INTERVAL_MS = 20000;
constexpr unsigned long NTP_RETRY_MS = 15000;
constexpr unsigned long DNS_TIMEOUT_MS = 3000;

IPAddress ownIp;
bool ntpStarted = false;
unsigned long lastNtpAttemptMs = 0;
bool wasConnected = false;
int lastStatus = -1;
bool wasTimeValid = false;
unsigned long lastAttemptMs = 0;

const char* statusName(int status)
{
    switch (status)
    {
    case WL_CONNECTED:
        return "connected";
    case WL_IDLE_STATUS:
        return "link down";
    case WL_DISCONNECTED:
        return "joined, no IP";
    case WL_CONNECT_FAILED:
        return "failed (wrong password, network not found or WPA3)";
    default:
        return "other";
    }
}

// NTP.begin(hostname) silently does nothing when the DNS lookup fails, so resolve here and fall back to a fixed IP.
void startNtp()
{
    IPAddress server;
    if (!WiFi.hostByName("pool.ntp.org", server, DNS_TIMEOUT_MS)
        && !WiFi.hostByName("time.google.com", server, DNS_TIMEOUT_MS))
    {
        Serial.println("NTP: DNS lookup failed, using fixed server IP");
        server = IPAddress(216, 239, 35, 0); // time.google.com
    }
    Serial.print("NTP: asking ");
    Serial.println(server.toString());
    NTP.begin(server);
}

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
    int status = WiFi.status();
    if (status != lastStatus)
    {
        lastStatus = status;
        Serial.print("WLAN status: ");
        Serial.print(statusName(status));
        Serial.print(" (");
        Serial.print(status);
        Serial.print(") after ");
        Serial.print(nowMs / 1000);
        Serial.println(" s");
    }

    bool connected = status == WL_CONNECTED;
    if (connected != wasConnected)
    {
        wasConnected = connected;
        if (connected)
        {
            Serial.print("WLAN connected, RSSI ");
            Serial.print(WiFi.RSSI());
            Serial.println(" dBm");
        }
        else
        {
            Serial.println("WLAN lost");
        }
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

    // Once synced, lwIP's SNTP keeps resyncing in the background.
    if (!timeReady() && (!ntpStarted || nowMs - lastNtpAttemptMs >= NTP_RETRY_MS))
    {
        ntpStarted = true;
        lastNtpAttemptMs = nowMs;
        startNtp();
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
