#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <string>

#include "codec.h"
#include "format.h"
#include "kasse_api.h"
#include "kasse_pages.h"
#include "kasse_view.h"
#include "net.h"
#include "plate.h"
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

// Result of the last POST, shown once after the redirect (post/redirect/get: reload doesn't pay twice).
uint32_t noticeId = 0;
Notice lastNotice;

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

void redirectWithNotice(const String& path, const std::string& plate, const char* cls, const std::string& text)
{
    lastNotice = {cls, text};
    noticeId++;
    String url = path + "?";
    if (isValidPlate(plate))
    {
        url += "kz=" + String(plate.c_str()) + "&";
    }
    url += "m=" + String(noticeId);
    server.sendHeader("Location", url);
    server.send(303);
    logRequest(303);
}

// The body of an application/json POST is in the "plain" argument.
void onApiStatus()
{
    sendApi(handleStatus(sessions, argOf("kz"), nowUtc()));
}

void onApiEntry()
{
    ApiResponse r = handleEntry(sessions, argOf("plain"), nowUtc(), timeReady());
    if (r.status == 201)
    {
        saveSessions();
    }
    sendApi(r);
}

void onApiExit()
{
    ApiResponse r = handleExit(sessions, argOf("plain"), nowUtc(), timeReady());
    if (r.status == 200)
    {
        saveSessions();
    }
    sendApi(r);
}

void onApiPay()
{
    ApiResponse r = handlePay(sessions, argOf("plain"), nowUtc(), timeReady());
    if (r.status == 200)
    {
        saveSessions();
    }
    sendApi(r);
}

void showPage(Page page)
{
    Notice notice;
    if (server.hasArg("m") && server.arg("m").toInt() == static_cast<long>(noticeId))
    {
        notice = lastNotice;
    }
    std::string html = renderPage(page, sessions, argOf("kz"), notice, nowUtc(), timeReady());
    server.send(200, "text/html; charset=utf-8", String(html.c_str()));
    logRequest(200);
}

void onPay(Page page)
{
    std::string plate = normalizePlate(argOf("kz"));
    PayFormResult r = handlePayForm(sessions, plate, argOf("method"), argOf("amount"), nowUtc(), timeReady());
    if (r.changed)
    {
        saveSessions();
    }
    Serial.print("[pay] ");
    Serial.print(plate.c_str());
    Serial.print(": ");
    Serial.println(r.notice.text.c_str());
    redirectWithNotice(pagePath(page), plate, r.notice.cls, r.notice.text);
}

void onReset()
{
    loadDefaultSessions(sessions);
    saveSessions();
    Serial.println("sessions: test-data reset");
    redirectWithNotice("/kasse", "", "ok", "Testdaten zurückgesetzt – keine Parkvorgänge, Transaktionszähler auf 1");
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
    server.on("/", HTTP_GET, []() {
        server.sendHeader("Location", "/kasse");
        server.send(303);
    });
    server.on("/kasse", HTTP_GET, []() { showPage(Page::Kasse); });
    server.on("/automat", HTTP_GET, []() { showPage(Page::Automat); });
    server.on("/tarif", HTTP_GET, []() {
        server.send(200, "text/html; charset=utf-8", String(renderTariffPage().c_str()));
        logRequest(200);
    });
    server.on("/kasse/pay", HTTP_POST, []() { onPay(Page::Kasse); });
    server.on("/automat/pay", HTTP_POST, []() { onPay(Page::Automat); });
    server.on("/reset", HTTP_POST, onReset);
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
