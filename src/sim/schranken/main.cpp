#include <cstdio>
#include <string>

#include "barrier.h"
#include "codec.h"
#include "event_log.h"
#include "format.h"
#include "gate.h"
#include "html.h"
#include "http_client.h"
#include "http_server.h"
#include "operator_view.h"
#include "registration.h"
#include "schranken_view.h"
#include "sim_common.h"
#include "terminal.h"
#include "testdata.h"

// Simulated Pico 2 "Schranken": registration and camera pages, barriers, real HTTP calls to the Kasse
// at KASSE_URL (default http://localhost:8081). Same routes as src/schranken/main.cpp, plus /sim, which
// stands in for the safety loop buttons. Pico 1 off = stop the Kasse program or container.
// Operator view /betrieb: user «betrieb», password OPERATOR_PASSWORD (default "parkhaus").

namespace
{
constexpr int KASSE_TIMEOUT_MS = 1000;

std::string kasseUrl;
std::string operatorPassword;

class HttpKasseLink : public KasseLink
{
public:
    HttpReply postJson(const char* path, const std::string& body) override
    {
        HttpReply r = httpPostJson(kasseUrl, path, body, KASSE_TIMEOUT_MS);
        std::printf("[kasse] POST %s %s -> %d %s\n", path, body.c_str(), r.status,
                    r.status > 0 ? r.body.c_str() : "(keine Antwort)");
        return r;
    }

    HttpReply getJson(const std::string& pathAndQuery) override
    {
        return httpGetJson(kasseUrl, pathAndQuery, KASSE_TIMEOUT_MS);
    }
};

struct Lane
{
    GateLane id;
    const char* name;
    Barrier barrier;
    bool loopOccupied = false;
    LaneView view;
    LaneTerminal terminal;
};

HttpKasseLink kasse;
RegistrationList registrations;
EventLog eventLog;
Lane entryLane{GateLane::Entry, "Einfahrt", {}, false, {}, {}};
Lane exitLane{GateLane::Exit, "Ausfahrt", {}, false, {}, {}};

// Result of the last registration / operator POST, shown once after the redirect.
uint32_t noticeId = 0;
Notice lastNotice;

void saveEvents()
{
    writeDataFile("events", eventsToJson(eventLog));
}

Notice pendingNotice(const HttpRequest& req)
{
    return req.arg("m") == std::to_string(noticeId) ? lastNotice : Notice{};
}

HttpResponse redirectWithNotice(const std::string& path, const Notice& notice)
{
    lastNotice = notice;
    noticeId++;
    return redirect(path + "?m=" + std::to_string(noticeId));
}

void saveRegistrations()
{
    writeDataFile("registrations", registrationsToJson(registrations));
}

// Shows a terminal result (gate decision, payment, call, abort) and opens the barrier if it says so.
void showOnTerminal(Lane& lane, const GateResult& r)
{
    if (r.open)
    {
        lane.barrier.open(nowMs());
    }
    if (logGateResult(eventLog, lane.id, r, nowUtc()))
    {
        saveEvents();
    }
    lane.view.cls = r.cls;
    lane.view.terminal = r.message;
    std::printf("[gate] %s %s: %s\n", lane.name, r.plate.c_str(), r.message.c_str());
}

void applyGate(Lane& lane, const GateResult& r)
{
    recordGate(lane.terminal, r);
    showOnTerminal(lane, r);
}

void refreshView(Lane& lane)
{
    lane.view.barrier = barrierStateLabel(lane.barrier.state());
    lane.view.fault = lane.barrier.loopFault() ? loopFaultMessage(lane.name) : "";
    lane.view.payPlate = lane.terminal.payPlate;
    lane.view.payFee = lane.terminal.payPlate.empty() ? "" : formatChf(lane.terminal.payFeeRappen);
}

HttpResponse finishRegistrationForm(const FormResult& r)
{
    if (r.changed)
    {
        saveRegistrations();
    }
    std::printf("[registration] %s\n", r.notice.text.c_str());
    return redirectWithNotice("/registrierung", r.notice);
}

OperatorLane operatorLane(Lane& lane)
{
    OperatorLane o;
    o.lane = lane.id;
    o.barrier = barrierStateLabel(lane.barrier.state());
    o.lastPlate = lane.terminal.lastPlate;
    o.status = laneStatus(kasse, lane.terminal.lastPlate);
    o.call = callLabel(lane.terminal);
    o.fault = lane.barrier.loopFault() ? loopFaultMessage(lane.name) : "";
    return o;
}

HttpResponse operatorRoute(const HttpRequest& req)
{
    if (!basicAuthOk(req, OPERATOR_USER, operatorPassword))
    {
        return basicAuthChallenge("Parkhaus Betrieb");
    }
    if (req.method == "GET")
    {
        OperatorLane entry = operatorLane(entryLane);
        OperatorLane exit = operatorLane(exitLane);
        return simPage(renderOperatorPage(entry, exit, eventLog, req.arg("filter"), pendingNotice(req)));
    }
    GateLane id = GateLane::None;
    gateLaneFromName(req.arg("lane"), id);
    RemoteOpenResult r = remoteOpen(id, req.arg("kz"), req.arg("reason"), nowUtc(), kasse, eventLog);
    if (r.open)
    {
        Lane& lane = id == GateLane::Entry ? entryLane : exitLane;
        lane.barrier.open(nowMs());
        recordRemoteOpening(lane.terminal);
    }
    if (r.logged)
    {
        saveEvents();
    }
    std::printf("[betrieb] %s\n", r.notice.text.c_str());
    return redirectWithNotice("/betrieb", r.notice);
}

void appendLoopControl(std::string& html, const char* field, const Lane& lane)
{
    html += "<tr><th>Sicherungsschleife ";
    html += lane.name;
    html += "</th><td>";
    html += lane.loopOccupied ? "<strong>belegt</strong>" : "frei";
    html += "</td><td><form method=\"post\" action=\"/sim/set\" class=\"inline\"><input type=\"hidden\" name=\"";
    html += field;
    html += "\" value=\"";
    html += lane.loopOccupied ? "0" : "1";
    html += "\"><button type=\"submit\">";
    html += lane.loopOccupied ? "freigeben" : "belegen";
    html += "</button></form></td><td>Schranke: ";
    html += barrierStateLabel(lane.barrier.state());
    html += "</td></tr>";
}

HttpResponse simControl()
{
    std::string html;
    appendPageStart(html, "Simulator",
                    "<a href=\"/kamera\">Kamerasimulation</a><a href=\"/registrierung\">Registrierung</a>",
                    "Simulator-Steuerung");
    html += "<p class=\"muted\">Ersetzt die Taster der Sicherungsschleifen.</p><div class=\"card\"><table>";
    appendLoopControl(html, "entryLoop", entryLane);
    appendLoopControl(html, "exitLoop", exitLane);
    html += "</table></div><p>Kasse: <code>" + htmlEscape(kasseUrl) +
            "</code>. Pico 1 «ausschalten»: Kasse-Programm beenden bzw. <code>docker compose stop kasse</code>.</p>"
            "<p><a href=\"/sim\">Aktualisieren</a></p>";
    appendPageEnd(html);
    return simPage(html);
}

HttpResponse route(const HttpRequest& req)
{
    bool get = req.method == "GET";
    bool post = req.method == "POST";
    if (get && req.path == "/")
    {
        return redirect("/kamera");
    }
    if (get && req.path == "/kamera")
    {
        refreshView(entryLane);
        refreshView(exitLane);
        return simPage(renderCameraPage(entryLane.view, exitLane.view));
    }
    if (post && req.path == "/kamera/einfahrt")
    {
        applyGate(entryLane, entryRequest(registrations, req.arg("kz"), req.arg("entry"), kasse));
        return redirect("/kamera");
    }
    if (post && req.path == "/kamera/ausfahrt")
    {
        applyGate(exitLane, exitRequest(req.arg("kz"), kasse));
        return redirect("/kamera");
    }
    if (post && req.path == "/kamera/ruftaste")
    {
        Lane& lane = req.arg("lane") == "exit" ? exitLane : entryLane;
        showOnTerminal(lane, pressCallButton(lane.terminal, nowUtc(), eventLog));
        saveEvents();
        return redirect("/kamera");
    }
    if (post && req.path == "/kamera/abbrechen")
    {
        showOnTerminal(exitLane, abortAtTerminal(exitLane.terminal));
        return redirect("/kamera");
    }
    if (post && req.path == "/kamera/bezahlen")
    {
        size_t eventsBefore = eventLog.all().size();
        showOnTerminal(exitLane, payAtTerminal(exitLane.terminal, req.arg("method"), nowUtc(), kasse, eventLog));
        if (eventLog.all().size() != eventsBefore)
        {
            saveEvents();
        }
        return redirect("/kamera");
    }
    if (get && req.path == "/registrierung")
    {
        return simPage(renderRegistrationPage(registrations, pendingNotice(req)));
    }
    if ((get && req.path == "/betrieb") || (post && req.path == "/betrieb/oeffnen"))
    {
        return operatorRoute(req);
    }
    if (post && req.path == "/registrierung/add")
    {
        return finishRegistrationForm(handleRegistrationForm(registrations, req.arg("name"), req.arg("kz")));
    }
    if (post && req.path == "/registrierung/delete")
    {
        return finishRegistrationForm(handleDeleteForm(registrations, req.arg("kz")));
    }
    if (post && req.path == "/reset")
    {
        loadDefaultRegistrations(registrations);
        saveRegistrations();
        eventLog.clear();
        saveEvents();
        return finishRegistrationForm(
            {{"ok", "Testdaten zurückgesetzt – Registrierungen KZ-01, KZ-02, KZ-08, KZ-10, Ereignisprotokoll leer"},
             false});
    }
    if (get && req.path == "/sim")
    {
        return simControl();
    }
    if (post && req.path == "/sim/set")
    {
        if (req.hasArg("entryLoop"))
        {
            entryLane.loopOccupied = req.arg("entryLoop") == "1";
        }
        if (req.hasArg("exitLoop"))
        {
            exitLane.loopOccupied = req.arg("exitLoop") == "1";
        }
        return redirect("/sim");
    }
    return notFound();
}

void updateLane(Lane& lane, uint32_t ms)
{
    BarrierState before = lane.barrier.state();
    BarrierEvent event = lane.barrier.update(ms, lane.loopOccupied);
    if (logBarrierEvent(eventLog, lane.id, event, nowUtc()))
    {
        saveEvents();
        std::printf("[safety] %s %s\n", eventTypeLabel(eventLog.all().back().type), lane.name);
    }
    if (lane.barrier.state() != before)
    {
        std::printf("[barrier] %s: %s\n", lane.name, barrierStateLabel(lane.barrier.state()));
    }
}

void tick()
{
    uint32_t ms = nowMs();
    updateLane(entryLane, ms);
    updateLane(exitLane, ms);
    std::fflush(stdout);
}

void loadRegistrations()
{
    makeDataDir();
    std::string json;
    if (!readDataFile("registrations", json) || !registrationsFromJson(json, registrations))
    {
        loadDefaultRegistrations(registrations);
        saveRegistrations();
        std::printf("registrations: defaults written\n");
        return;
    }
    std::printf("registrations: loaded %u\n", static_cast<unsigned>(registrations.all().size()));
}

void loadEvents()
{
    std::string json;
    if (!readDataFile("events", json) || !eventsFromJson(json, eventLog))
    {
        eventLog.clear();
        saveEvents();
    }
    std::printf("events: %u\n", static_cast<unsigned>(eventLog.all().size()));
}
}

int main()
{
    kasseUrl = envOr("KASSE_URL", "http://localhost:8081");
    operatorPassword = envOr("OPERATOR_PASSWORD", "parkhaus");
    entryLane.terminal.lane = GateLane::Entry;
    exitLane.terminal.lane = GateLane::Exit;
    loadRegistrations();
    loadEvents();
    std::string host = listenHost();
    std::printf("Simulator Pico 2 «Schranken», %s, Daten in %s/, Kasse unter %s\n", formatDateTime(nowUtc()).c_str(),
                dataDir().c_str(), kasseUrl.c_str());
    std::printf("  http://localhost:%u/kamera   http://localhost:%u/registrierung   http://localhost:%u/sim"
                "   (lauscht auf %s)\n",
                SCHRANKEN_PORT, SCHRANKEN_PORT, SCHRANKEN_PORT, host.c_str());
    std::printf("  http://localhost:%u/betrieb   (Benutzer «betrieb», Passwort aus OPERATOR_PASSWORD)\n", SCHRANKEN_PORT);
    std::printf("Beenden mit Ctrl+C\n");
    std::fflush(stdout);
    return runHttpServers(host, {{SCHRANKEN_PORT, route}}, tick) ? 0 : 1;
}
