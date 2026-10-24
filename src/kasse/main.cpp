#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <string>

#include "codec.h"
#include "format.h"
#include "kasse_api.h"
#include "kasse_pages.h"
#include "net.h"
#include "secrets.h"
#include "status_led.h"
#include "storage.h"
#include "testdata.h"

namespace
{
constexpr unsigned long REPORT_INTERVAL_MS = 30000;

WebServer server(80);
bool serverStarted = false;
SessionStore sessions;
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

void logRequest(int status)
{
    Serial.print("[http] ");
    Serial.print(server.method() == HTTP_GET ? "GET " : "POST ");
    Serial.print(server.uri());
    if (server.hasArg("kz"))
    {
        Serial.print(" kz=");
        Serial.print(server.arg("kz"));
    }
    Serial.print(" -> ");
    Serial.println(status);
}

void sendApi(const ApiResponse& r)
{
    server.send(r.status, "application/json", String(r.body.c_str()));
    logRequest(r.status);
}

std::string argOf(const char* name)
{
    return server.arg(name).c_str();
}

// API call that changes the sessions on success.
void sendApiChange(const ApiResponse& r, int okStatus)
{
    if (r.status == okStatus)
    {
        saveSessions();
    }
    sendApi(r);
}

// The body of an application/json POST is in the "plain" argument.
void onApiStatus()
{
    sendApi(handleStatus(sessions, argOf("kz"), nowUtc()));
}

void onApiSessions()
{
    sendApi(handleSessions(sessions, nowUtc(), timeReady()));
}

void onApiEntry()
{
    sendApiChange(handleEntry(sessions, argOf("plain"), nowUtc(), timeReady()), 201);
}

void onApiExit()
{
    sendApiChange(handleExit(sessions, argOf("plain"), nowUtc(), timeReady()), 200);
}

void onApiPay()
{
    sendApiChange(handlePay(sessions, argOf("plain"), nowUtc(), timeReady()), 200);
}

void onApiReset()
{
    Serial.println("sessions: test-data reset");
    sendApiChange(handleReset(sessions), 200);
}

// The pages are on Pico 2.
void onRoot()
{
    String uiUrl = String("http://") + IPAddress(SCHRANKEN_IP).toString();
    server.send(200, "text/html; charset=utf-8", String(renderKasseServerPage(uiUrl.c_str()).c_str()));
    logRequest(200);
}

void onNotFound()
{
    server.send(404, "text/plain; charset=utf-8", "Nicht gefunden");
    logRequest(404);
}

void setupRoutes()
{
    server.on("/api/status", HTTP_GET, onApiStatus);
    server.on("/api/entry", HTTP_POST, onApiEntry);
    server.on("/api/exit", HTTP_POST, onApiExit);
    server.on("/api/pay", HTTP_POST, onApiPay);
    server.on("/api/sessions", HTTP_GET, onApiSessions);
    server.on("/api/reset", HTTP_POST, onApiReset);
    server.on("/", HTTP_GET, onRoot);
    server.onNotFound(onNotFound);
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
    setupRoutes();
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
