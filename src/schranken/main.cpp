#include <Arduino.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFi.h>
#include <string>
#include <vector>

#include "format.h"
#include "gate.h"
#include "lane.h"
#include "net.h"
#include "pins.h"
#include "schranken_app.h"
#include "secrets.h"
#include "status_led.h"
#include "storage.h"
#include "web_glue.h"

// Pico 2 "Schranken": lib/schranken_app (all pages and routes) on core 0, the two barriers on core 1 (lane.h).

#ifndef OPERATOR_PASSWORD
#error "OPERATOR_PASSWORD missing in include/secrets.h (see include/secrets.example.h)"
#endif

namespace
{
constexpr unsigned long REPORT_INTERVAL_MS = 30000;
constexpr uint16_t KASSE_TIMEOUT_MS = 1000;

// Calls Pico 1 over HTTP. Any transport error is reported as status <= 0 (= unreachable).
class HttpKasseLink : public KasseLink
{
public:
    HttpReply postJson(const char* path, const std::string& body) override
    {
        return request(path, &body);
    }

    HttpReply getJson(const std::string& pathAndQuery) override
    {
        return request(pathAndQuery.c_str(), nullptr);
    }

private:
    // POST if body is given, GET otherwise.
    HttpReply request(const char* path, const std::string* body)
    {
        if (!wifiReady())
        {
            return {};
        }
        WiFiClient client;
        HTTPClient http;
        http.setTimeout(KASSE_TIMEOUT_MS);
        String url = String("http://") + IPAddress(KASSE_IP).toString() + path;
        if (!http.begin(client, url))
        {
            return {};
        }
        HttpReply reply;
        if (body != nullptr)
        {
            http.addHeader("Content-Type", "application/json");
            reply.status = http.POST(reinterpret_cast<const uint8_t*>(body->data()), body->size());
        }
        else
        {
            reply.status = http.GET();
        }
        if (reply.status > 0)
        {
            reply.body = http.getString().c_str();
        }
        http.end();
        return reply;
    }
};

class BoardPlatform : public AppPlatform
{
public:
    bool readFile(const char* name, std::string& json) override
    {
        return storageRead(name, json);
    }

    void writeFile(const char* name, const std::string& json) override
    {
        if (!storageWrite(name, json))
        {
            Serial.printf("%s: write FAILED\n", name);
        }
    }

    uint32_t random32() override
    {
        return rp2040.hwrand32();
    }

    void log(const std::string& line) override
    {
        Serial.println(line.c_str());
    }
};

WebServer server(80);
bool serverStarted = false;
HttpKasseLink kasse;
BoardPlatform platform;
CoreLane entryLane{GateLane::Entry, PIN_ENTRY_SERVO, PIN_ENTRY_LOOP};
CoreLane exitLane{GateLane::Exit, PIN_EXIT_SERVO, PIN_EXIT_LOOP};
SchrankenApp app{kasse, entryLane, exitLane, platform, OPERATOR_PASSWORD};
unsigned long lastReportMs = 0;

void handleRequest()
{
    unsigned long startMs = millis();
    WebRequest req = readRequest(server);
    WebResponse res = app.handle(req, nowUtc());
    sendResponse(server, req, res, startMs);
}

// Core 0: logs what core 1 reported, with the NTP time.
void drainLane(CoreLane& lane, BarrierState& lastState)
{
    std::vector<BarrierEvent> events;
    BarrierEvent event;
    while (lane.popEvent(event))
    {
        events.push_back(event);
    }
    if (!events.empty())
    {
        app.onBarrierEvents(lane.id, events, nowUtc());
    }
    BarrierState state = lane.state();
    if (state != lastState)
    {
        lastState = state;
        Serial.printf("[barrier] %s: %s\n", gateLaneLabel(lane.id), barrierStateLabel(state));
    }
}

void report()
{
    Serial.print("[schranken] WLAN ");
    Serial.print(wifiReady() ? WiFi.localIP().toString() : String("not connected"));
    Serial.print(" | time ");
    Serial.print(timeReady() ? formatDateTime(nowUtc()).c_str() : "not synced");
    Serial.print(" | registrations ");
    Serial.print(app.registrations().all().size());
    Serial.print(" | lost barrier events ");
    Serial.println(entryLane.droppedEvents() + exitLane.droppedEvents());
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
    app.begin();
    server.onNotFound(handleRequest);
    server.collectHeaders("Cookie");
    netBegin(IPAddress(SCHRANKEN_IP));
}

void loop()
{
    static BarrierState entryState = BarrierState::Closed;
    static BarrierState exitState = BarrierState::Closed;
    unsigned long nowMs = millis();
    netUpdate(nowMs);
    statusLedUpdate(wifiReady() && timeReady(), nowMs);
    drainLane(entryLane, entryState);
    drainLane(exitLane, exitState);

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

void setup1()
{
    entryLane.begin();
    exitLane.begin();
}

void loop1()
{
    uint32_t nowMs = millis();
    entryLane.update(nowMs);
    exitLane.update(nowMs);
}
