#include <Arduino.h>
#include <HTTPClient.h>
#include <Servo.h>
#include <WebServer.h>
#include <WiFi.h>
#include <atomic>
#include <string>

#include "barrier.h"
#include "codec.h"
#include "event_log.h"
#include "format.h"
#include "gate.h"
#include "net.h"
#include "operator_view.h"
#include "pins.h"
#include "schranken_view.h"
#include "secrets.h"
#include "status_led.h"
#include "storage.h"
#include "terminal.h"
#include "testdata.h"

#ifndef OPERATOR_PASSWORD
#warning "OPERATOR_PASSWORD missing in include/secrets.h, using the example password"
#define OPERATOR_PASSWORD "parkhaus"
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
            reply.status = http.POST(String(body->c_str()));
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

// Barrier events from core 1 to core 0 (single producer, single consumer). Core 0 logs them with the NTP
// time and writes the log to flash; core 1 must not touch the log or LittleFS.
struct BarrierEventQueue
{
    static constexpr uint32_t SIZE = 8;
    BarrierEvent items[SIZE] = {};
    std::atomic<uint32_t> head{0}; // written by core 1
    std::atomic<uint32_t> tail{0}; // written by core 0

    void push(BarrierEvent e)
    {
        uint32_t h = head.load();
        if (h - tail.load() < SIZE)
        {
            items[h % SIZE] = e;
            head.store(h + 1);
        }
    }

    bool pop(BarrierEvent& e)
    {
        uint32_t t = tail.load();
        if (t == head.load())
        {
            return false;
        }
        e = items[t % SIZE];
        tail.store(t + 1);
        return true;
    }
};

// Barriers and loops run on core 1 (setup1/loop1), so a blocking HTTP call on core 0 never delays a
// safety stop. The cores share only the atomics below, each written by one core.
struct Lane
{
    Lane(GateLane laneId, uint8_t servo, uint8_t loop)
        : id(laneId), name(gateLaneLabel(laneId)), servoPin(servo), loopPin(loop)
    {
        terminal.lane = laneId;
    }

    GateLane id;
    const char* name;
    uint8_t servoPin;
    uint8_t loopPin;

    // core 0 -> core 1: incremented for every open command
    std::atomic<uint32_t> openRequests{0};
    // core 1 -> core 0
    std::atomic<int> state{static_cast<int>(BarrierState::Closed)};
    std::atomic<bool> fault{false};
    BarrierEventQueue events;

    // core 1 only
    Servo servo;
    Barrier barrier;
    uint32_t seenOpenRequests = 0;
    int writtenAngle = -1;
    BarrierState loggedState = BarrierState::Closed;

    // core 0 only
    LaneView view;
    LaneTerminal terminal;
};

WebServer server(80);
bool serverStarted = false;
HttpKasseLink kasse;
RegistrationList registrations;
EventLog eventLog;
Lane entryLane{GateLane::Entry, PIN_ENTRY_SERVO, PIN_ENTRY_LOOP};
Lane exitLane{GateLane::Exit, PIN_EXIT_SERVO, PIN_EXIT_LOOP};
unsigned long lastReportMs = 0;

// Result of the last registration / operator POST, shown once after the redirect.
uint32_t noticeId = 0;
Notice lastNotice;

void saveEvents()
{
    if (!storageWrite("events", eventsToJson(eventLog)))
    {
        Serial.println("events: write FAILED");
    }
}

void loadEvents()
{
    std::string json;
    if (!storageRead("events", json) || !eventsFromJson(json, eventLog))
    {
        eventLog.clear();
        saveEvents();
        Serial.println("events: empty log written");
        return;
    }
    Serial.print("events: loaded ");
    Serial.println(eventLog.all().size());
}

void saveRegistrations()
{
    if (!storageWrite("registrations", registrationsToJson(registrations)))
    {
        Serial.println("registrations: write FAILED");
    }
}

void loadRegistrations()
{
    std::string json;
    if (!storageRead("registrations", json) || !registrationsFromJson(json, registrations))
    {
        loadDefaultRegistrations(registrations);
        saveRegistrations();
        Serial.println("registrations: defaults written");
        return;
    }
    Serial.print("registrations: loaded ");
    Serial.println(registrations.all().size());
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

std::string argOf(const char* name)
{
    return server.arg(name).c_str();
}

void redirect(const String& url)
{
    server.sendHeader("Location", url);
    server.send(303);
    logRequest(303);
}

void sendHtml(const std::string& html)
{
    server.send(200, "text/html; charset=utf-8", String(html.c_str()));
    logRequest(200);
}

void driveServo(Lane& lane)
{
    int angle = lane.barrier.angle();
    if (angle != lane.writtenAngle)
    {
        lane.servo.write(angle);
        lane.writtenAngle = angle;
    }
    if (lane.barrier.state() != lane.loggedState)
    {
        lane.loggedState = lane.barrier.state();
        Serial.print("[barrier] ");
        Serial.print(lane.name);
        Serial.print(": ");
        Serial.println(barrierStateLabel(lane.loggedState));
    }
}

void openBarrier(Lane& lane)
{
    lane.openRequests.store(lane.openRequests.load() + 1);
}

// Shows a terminal result (gate decision, payment, call, abort) and opens the barrier if it says so.
void showOnTerminal(Lane& lane, const GateResult& r)
{
    if (r.open)
    {
        openBarrier(lane);
    }
    if (logGateResult(eventLog, lane.id, r, nowUtc()))
    {
        saveEvents();
    }
    lane.view.cls = r.cls;
    lane.view.terminal = r.message;
    Serial.print("[gate] ");
    Serial.print(lane.name);
    Serial.print(" ");
    Serial.print(r.plate.c_str());
    Serial.print(": ");
    Serial.println(r.message.c_str());
}

void applyGate(Lane& lane, const GateResult& r)
{
    recordGate(lane.terminal, r);
    showOnTerminal(lane, r);
}

Lane* laneArg()
{
    GateLane id = GateLane::None;
    gateLaneFromName(argOf("lane"), id);
    return id == GateLane::Entry ? &entryLane : id == GateLane::Exit ? &exitLane : nullptr;
}

void onCallButton()
{
    Lane* lane = laneArg();
    if (lane != nullptr)
    {
        showOnTerminal(*lane, pressCallButton(lane->terminal, nowUtc(), eventLog));
        saveEvents();
    }
    redirect("/kamera");
}

void onAbort()
{
    showOnTerminal(exitLane, abortAtTerminal(exitLane.terminal));
    redirect("/kamera");
}

void onTerminalPay()
{
    size_t eventsBefore = eventLog.all().size();
    showOnTerminal(exitLane, payAtTerminal(exitLane.terminal, argOf("method"), nowUtc(), kasse, eventLog));
    if (eventLog.all().size() != eventsBefore)
    {
        saveEvents();
    }
    redirect("/kamera");
}

void onEntry()
{
    unsigned long startMs = millis();
    applyGate(entryLane, entryRequest(registrations, argOf("kz"), argOf("entry"), kasse));
    Serial.print("[gate] decision after ");
    Serial.print(millis() - startMs);
    Serial.println(" ms");
    redirect("/kamera");
}

void onExit()
{
    unsigned long startMs = millis();
    applyGate(exitLane, exitRequest(argOf("kz"), kasse));
    Serial.print("[gate] decision after ");
    Serial.print(millis() - startMs);
    Serial.println(" ms");
    redirect("/kamera");
}

void refreshView(Lane& lane)
{
    lane.view.barrier = barrierStateLabel(static_cast<BarrierState>(lane.state.load()));
    lane.view.fault = lane.fault.load() ? loopFaultMessage(lane.name) : "";
    lane.view.payPlate = lane.terminal.payPlate;
    lane.view.payFee = lane.terminal.payPlate.empty() ? "" : formatChf(lane.terminal.payFeeRappen);
}

void showCamera()
{
    refreshView(entryLane);
    refreshView(exitLane);
    sendHtml(renderCameraPage(entryLane.view, exitLane.view));
}

Notice pendingNotice()
{
    if (server.hasArg("m") && server.arg("m").toInt() == static_cast<long>(noticeId))
    {
        return lastNotice;
    }
    return {};
}

void redirectWithNotice(const String& path, const Notice& notice)
{
    lastNotice = notice;
    noticeId++;
    redirect(path + "?m=" + String(noticeId));
}

void showRegistration()
{
    sendHtml(renderRegistrationPage(registrations, pendingNotice()));
}

// HTTP Basic auth, user «betrieb». False = 401 sent, stop handling.
bool operatorAuthenticated()
{
    if (server.authenticate(OPERATOR_USER, OPERATOR_PASSWORD))
    {
        return true;
    }
    server.requestAuthentication(BASIC_AUTH, "Parkhaus Betrieb");
    logRequest(401);
    return false;
}

OperatorLane operatorLane(Lane& lane)
{
    OperatorLane o;
    o.lane = lane.id;
    o.barrier = barrierStateLabel(static_cast<BarrierState>(lane.state.load()));
    o.lastPlate = lane.terminal.lastPlate;
    o.status = laneStatus(kasse, lane.terminal.lastPlate);
    o.fault = lane.fault.load() ? loopFaultMessage(lane.name) : "";
    o.call = callLabel(lane.terminal);
    return o;
}

void showOperator()
{
    if (!operatorAuthenticated())
    {
        return;
    }
    OperatorLane entry = operatorLane(entryLane);
    OperatorLane exit = operatorLane(exitLane);
    sendHtml(renderOperatorPage(entry, exit, eventLog, argOf("filter"), pendingNotice()));
}

void onRemoteOpen()
{
    if (!operatorAuthenticated())
    {
        return;
    }
    GateLane id = GateLane::None;
    gateLaneFromName(argOf("lane"), id);
    RemoteOpenResult r = remoteOpen(id, argOf("kz"), argOf("reason"), nowUtc(), kasse, eventLog);
    if (r.open)
    {
        Lane& lane = id == GateLane::Entry ? entryLane : exitLane;
        openBarrier(lane);
        recordRemoteOpening(lane.terminal);
    }
    if (r.logged)
    {
        saveEvents();
    }
    Serial.print("[betrieb] ");
    Serial.println(r.notice.text.c_str());
    redirectWithNotice("/betrieb", r.notice);
}

void finishRegistrationForm(const FormResult& r)
{
    if (r.changed)
    {
        saveRegistrations();
    }
    Serial.print("[registration] ");
    Serial.println(r.notice.text.c_str());
    redirectWithNotice("/registrierung", r.notice);
}

void onReset()
{
    loadDefaultRegistrations(registrations);
    saveRegistrations();
    eventLog.clear();
    saveEvents();
    Serial.println("registrations + events: test-data reset");
    finishRegistrationForm(
        {{"ok", "Testdaten zurückgesetzt – Registrierungen KZ-01, KZ-02, KZ-08, KZ-10, Ereignisprotokoll leer"}, false});
}

void onNotFound()
{
    server.send(404, "text/plain; charset=utf-8", "Nicht gefunden");
    logRequest(404);
}

void setupRoutes()
{
    server.on("/", HTTP_GET, []() { redirect("/kamera"); });
    server.on("/kamera", HTTP_GET, showCamera);
    server.on("/kamera/einfahrt", HTTP_POST, onEntry);
    server.on("/kamera/ausfahrt", HTTP_POST, onExit);
    server.on("/kamera/ruftaste", HTTP_POST, onCallButton);
    server.on("/kamera/abbrechen", HTTP_POST, onAbort);
    server.on("/kamera/bezahlen", HTTP_POST, onTerminalPay);
    server.on("/registrierung", HTTP_GET, showRegistration);
    server.on("/registrierung/add", HTTP_POST,
              []() { finishRegistrationForm(handleRegistrationForm(registrations, argOf("name"), argOf("kz"))); });
    server.on("/registrierung/delete", HTTP_POST,
              []() { finishRegistrationForm(handleDeleteForm(registrations, argOf("kz"))); });
    server.on("/reset", HTTP_POST, onReset);
    server.on("/betrieb", HTTP_GET, showOperator);
    server.on("/betrieb/oeffnen", HTTP_POST, onRemoteOpen);
    server.onNotFound(onNotFound);
}

void beginLane(Lane& lane)
{
    pinMode(lane.loopPin, INPUT_PULLUP);
    lane.servo.attach(lane.servoPin);
    driveServo(lane);
}

void updateLane(Lane& lane, uint32_t nowMs)
{
    uint32_t requests = lane.openRequests.load();
    if (requests != lane.seenOpenRequests)
    {
        lane.seenOpenRequests = requests;
        lane.barrier.open(nowMs);
    }
    bool occupied = digitalRead(lane.loopPin) == LOW;
    BarrierEvent event = lane.barrier.update(nowMs, occupied);
    driveServo(lane);
    if (event != BarrierEvent::None)
    {
        lane.events.push(event);
    }
    lane.state.store(static_cast<int>(lane.barrier.state()));
    lane.fault.store(lane.barrier.loopFault());
}

// Core 0: logs barrier events handed over by core 1.
void drainBarrierEvents(Lane& lane)
{
    BarrierEvent event;
    while (lane.events.pop(event))
    {
        logBarrierEvent(eventLog, lane.id, event, nowUtc());
        saveEvents();
        Serial.print("[safety] ");
        Serial.print(eventTypeLabel(eventLog.all().back().type));
        Serial.print(" ");
        Serial.println(lane.name);
    }
}

void report()
{
    Serial.print("[schranken] WLAN ");
    Serial.print(wifiReady() ? WiFi.localIP().toString() : String("not connected"));
    Serial.print(" | time ");
    Serial.print(timeReady() ? formatDateTime(nowUtc()).c_str() : "not synced");
    Serial.print(" | registrations ");
    Serial.println(registrations.all().size());
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
    loadRegistrations();
    loadEvents();
    setupRoutes();
    netBegin(IPAddress(SCHRANKEN_IP));
}

void loop()
{
    unsigned long nowMs = millis();
    netUpdate(nowMs);
    statusLedUpdate(wifiReady() && timeReady(), nowMs);
    drainBarrierEvents(entryLane);
    drainBarrierEvents(exitLane);

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
    beginLane(entryLane);
    beginLane(exitLane);
}

void loop1()
{
    uint32_t nowMs = millis();
    updateLane(entryLane, nowMs);
    updateLane(exitLane, nowMs);
}
