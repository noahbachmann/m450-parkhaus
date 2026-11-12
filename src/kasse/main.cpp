#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <string>

#include "codec.h"
#include "format.h"
#include "kasse_api.h"
#include "net.h"
#include "secrets.h"
#include "status_led.h"
#include "storage.h"
#include "testdata.h"
#include "web_glue.h"

// Pico 1 "Kasse": the routes of lib/kasse_api (routeKasse), sessions in /sessions.json. The pages are on Pico 2.

#ifndef KASSE_API_KEY
#error "KASSE_API_KEY missing in include/secrets.h (see include/secrets.example.h)"
#endif

namespace
{
constexpr unsigned long REPORT_INTERVAL_MS = 30000;

WebServer server(80);
bool serverStarted = false;
SessionStore sessions;
std::string uiUrl;
unsigned long lastReportMs = 0;

void saveSessions()
{
    if (!storageWrite("sessions", sessionsToJson(sessions)))
    {
        Serial.println("sessions: write FAILED");
    }
}

void loadSessions()
{
    std::string json;
    if (!storageRead("sessions", json) || !sessionsFromJson(json, sessions))
    {
        loadDefaultSessions(sessions);
        saveSessions();
        Serial.println("sessions: defaults written");
        return;
    }
    Serial.print("sessions: loaded ");
    Serial.println(sessions.all().size());
}

void handleRequest()
{
    unsigned long startMs = millis();
    WebRequest req = readRequest(server);
    bool fromSchrankenIp = server.client().remoteIP() == IPAddress(SCHRANKEN_IP);
    KasseRouteResult r = routeKasse(req, sessions, nowUtc(), timeReady(), uiUrl, fromSchrankenIp, KASSE_API_KEY);
    if (r.changed)
    {
        saveSessions();
    }
    sendResponse(server, req, r.response, startMs);
}

void report()
{
    Serial.print("[kasse] WLAN ");
    Serial.print(wifiReady() ? WiFi.localIP().toString() : String("not connected"));
    Serial.print(" | time ");
    Serial.print(timeReady() ? formatDateTime(nowUtc()).c_str() : "not synced");
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
    loadSessions();
    uiUrl = (String("http://") + IPAddress(SCHRANKEN_IP).toString()).c_str();
    server.onNotFound(handleRequest);
    server.collectHeaders(API_KEY_HEADER);
    netBegin(IPAddress(KASSE_IP));
}

void loop()
{
    unsigned long nowMs = millis();
    netUpdate(nowMs);
    statusLedUpdate(wifiReady() && timeReady(), nowMs);

    if (!serverStarted && wifiReady())
    {
        server.begin();
        serverStarted = true;
        Serial.println("HTTP server listening on port 80");
    }
    if (serverStarted)
    {
        server.handleClient();
    }

    if (nowMs - lastReportMs >= REPORT_INTERVAL_MS)
    {
        lastReportMs = nowMs;
        report();
    }
}
