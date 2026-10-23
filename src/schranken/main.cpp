#include <Arduino.h>
#include <HTTPClient.h>
#include <Servo.h>
#include <WebServer.h>
#include <WiFi.h>
#include <atomic>
#include <string>

#include "barrier.h"
#include "codec.h"
#include "format.h"
#include "gate.h"
#include "net.h"
#include "pins.h"
#include "schranken_view.h"
#include "secrets.h"
#include "status_led.h"
#include "storage.h"
#include "testdata.h"

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
        http.addHeader("Content-Type", "application/json");
        HttpReply reply;
        reply.status = http.POST(String(body.c_str()));
        if (reply.status > 0)
        {
            reply.body = http.getString().c_str();
        }
        http.end();
        return reply;
    }
};

// Barriers and loops run on core 1 (setup1/loop1), so a blocking HTTP call on core 0 never delays a
// safety stop. The cores share only the atomics below, each written by one core.
struct Lane
{
    Lane(const char* laneName, uint8_t servo, uint8_t loop) : name(laneName), servoPin(servo), loopPin(loop)
    {
    }

    const char* name;
    uint8_t servoPin;
    uint8_t loopPin;

    // core 0 -> core 1: incremented for every open command
    std::atomic<uint32_t> openRequests{0};
    // core 1 -> core 0
    std::atomic<int> state{static_cast<int>(BarrierState::Closed)};
    std::atomic<bool> fault{false};

    // core 1 only
    Servo servo;
    Barrier barrier;
    uint32_t seenOpenRequests = 0;
    int writtenAngle = -1;
    BarrierState loggedState = BarrierState::Closed;

    // core 0 only
    LaneView view;
};

WebServer server(80);
bool serverStarted = false;
HttpKasseLink kasse;
RegistrationList registrations;
Lane entryLane{"Einfahrt", PIN_ENTRY_SERVO, PIN_ENTRY_LOOP};
Lane exitLane{"Ausfahrt", PIN_EXIT_SERVO, PIN_EXIT_LOOP};
unsigned long lastReportMs = 0;

// Result of the last registration POST, shown once after the redirect.
uint32_t noticeId = 0;
Notice lastNotice;

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

void applyGate(Lane& lane, const GateResult& r)
{
    if (r.open)
    {
        lane.openRequests.store(lane.openRequests.load() + 1);
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
}

void showCamera()
{
    refreshView(entryLane);
    refreshView(exitLane);
    sendHtml(renderCameraPage(entryLane.view, exitLane.view));
}

void showRegistration()
{
    Notice notice;
    if (server.hasArg("m") && server.arg("m").toInt() == static_cast<long>(noticeId))
    {
        notice = lastNotice;
    }
    sendHtml(renderRegistrationPage(registrations, notice));
}

void finishRegistrationForm(const FormResult& r)
{
    if (r.changed)
    {
        saveRegistrations();
    }
    Serial.print("[registration] ");
    Serial.println(r.notice.text.c_str());
    lastNotice = r.notice;
    noticeId++;
    redirect("/registrierung?m=" + String(noticeId));
}

void onReset()
{
    loadDefaultRegistrations(registrations);
    saveRegistrations();
    Serial.println("registrations: test-data reset");
    finishRegistrationForm({{"ok", "Testdaten zurückgesetzt – Registrierungen KZ-01, KZ-02, KZ-08, KZ-10"}, false});
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
    server.on("/registrierung", HTTP_GET, showRegistration);
    server.on("/registrierung/add", HTTP_POST,
              []() { finishRegistrationForm(handleRegistrationForm(registrations, argOf("name"), argOf("kz"))); });
    server.on("/registrierung/delete", HTTP_POST,
              []() { finishRegistrationForm(handleDeleteForm(registrations, argOf("kz"))); });
    server.on("/reset", HTTP_POST, onReset);
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
    if (event == BarrierEvent::SafetyStop)
    {
        Serial.print("[safety] Sicherheitsabschaltung ");
        Serial.println(lane.name);
    }
    else if (event == BarrierEvent::LoopFault)
    {
        Serial.print("[safety] ");
        Serial.println(loopFaultMessage(lane.name).c_str());
    }
    driveServo(lane);
    lane.state.store(static_cast<int>(lane.barrier.state()));
    lane.fault.store(lane.barrier.loopFault());
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
    setupRoutes();
    netBegin(IPAddress(SCHRANKEN_IP));
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
