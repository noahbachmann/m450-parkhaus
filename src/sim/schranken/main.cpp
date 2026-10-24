#include <cstdio>
#include <random>
#include <string>

#include "auth.h"
#include "barrier.h"
#include "codec.h"
#include "event_log.h"
#include "format.h"
#include "gate.h"
#include "html.h"
#include "http_client.h"
#include "http_server.h"
#include "kasse_client.h"
#include "kasse_pages.h"
#include "operator_view.h"
#include "plate.h"
#include "registration.h"
#include "schranken_view.h"
#include "sim_common.h"
#include "terminal.h"
#include "testdata.h"

// Simulated Pico 2 "Schranken": all pages (camera, Tarif; after login Kasse, registration, Betrieb), barriers,
// real HTTP calls to the Kasse at KASSE_URL (default http://localhost:8081). Same routes as
// src/schranken/main.cpp. No safety loops: «Durchfahren» and «Fahrzeug bleibt stehen» on the camera page
// stand in for them.
// Pico 1 off = stop the Kasse program or container. Login: user «betrieb», password OPERATOR_PASSWORD
// (default "parkhaus").

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
    LaneView view;
    LaneTerminal terminal;
    bool loopHeld = false; // «Fahrzeug bleibt stehen»
};

HttpKasseLink kasse;
RegistrationList registrations;
EventLog eventLog;
LoginSessions logins;
std::random_device randomSource;
Lane entryLane{GateLane::Entry, "Einfahrt", {}, {}, {}};
Lane exitLane{GateLane::Exit, "Ausfahrt", {}, {}, {}};

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

// query: extra arguments, e.g. "kz=LU123456&".
HttpResponse redirectWithNotice(const std::string& path, const Notice& notice, const std::string& query = "")
{
    lastNotice = notice;
    noticeId++;
    return redirect(path + "?" + query + "m=" + std::to_string(noticeId));
}

Role roleOf(const HttpRequest& req)
{
    return logins.valid(req.cookie) ? Role::Admin : Role::Public;
}

HttpResponse loginRequired(const HttpRequest& req)
{
    return redirect("/login?next=" + loginTarget(req.path));
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

Lane* laneOf(const HttpRequest& req)
{
    GateLane id = GateLane::None;
    gateLaneFromName(req.arg("lane"), id);
    return id == GateLane::Entry ? &entryLane : id == GateLane::Exit ? &exitLane : nullptr;
}

void refreshView(Lane& lane)
{
    if (lane.barrier.state() == BarrierState::Closed && lane.terminal.passing)
    {
        showOnTerminal(lane, closedAfterPass(lane.terminal));
    }
    lane.view.canPass = lane.barrier.state() == BarrierState::Open && !lane.terminal.passing && !lane.loopHeld;
    lane.view.canHold = lane.barrier.state() != BarrierState::Closed && !lane.loopHeld;
    lane.view.loopHeld = lane.loopHeld;
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

HttpResponse onRemoteOpen(const HttpRequest& req)
{
    GateLane id = GateLane::None;
    gateLaneFromName(req.arg("lane"), id);
    RemoteOpenResult r =
        remoteOpen(id, req.arg("kz"), req.arg("reason"), req.arg("name"), nowUtc(), kasse, registrations, eventLog);
    if (r.registered)
    {
        saveRegistrations();
    }
    if (r.open)
    {
        Lane& lane = id == GateLane::Entry ? entryLane : exitLane;
        lane.barrier.open(nowMs());
        recordRemoteOpening(lane.terminal, r.plate);
    }
    if (r.logged)
    {
        saveEvents();
    }
    std::printf("[betrieb] %s\n", r.notice.text.c_str());
    return redirectWithNotice("/betrieb", r.notice);
}

HttpResponse onLogin(const HttpRequest& req)
{
    std::string next = loginTarget(req.arg("next"));
    if (!credentialsOk(req.arg("user"), req.arg("password"), operatorPassword))
    {
        std::printf("[login] rejected\n");
        return redirectWithNotice("/login", {"err", "Benutzer oder Passwort falsch"}, "next=" + next + "&");
    }
    uint32_t words[4];
    for (uint32_t& w : words)
    {
        w = randomSource();
    }
    std::string token = makeToken(words);
    logins.add(token);
    std::printf("[login] %s\n", OPERATOR_USER);
    HttpResponse r = redirect(next);
    r.setCookie = loginCookie(token);
    return r;
}

HttpResponse onLogout(const HttpRequest& req)
{
    logins.remove(cookieValue(req.cookie, SESSION_COOKIE));
    HttpResponse r = redirect("/kamera");
    r.setCookie = logoutCookie();
    return r;
}

HttpResponse onReset()
{
    loadDefaultRegistrations(registrations);
    saveRegistrations();
    eventLog.clear();
    saveEvents();
    Notice notice{"ok", "Testdaten zurückgesetzt – Registrierungen KZ-01, KZ-02, KZ-08, KZ-10, Ereignisprotokoll leer, "
                        "keine Parkvorgänge"};
    if (!resetKasse(kasse))
    {
        notice = {"err", "Registrierungen und Ereignisprotokoll zurückgesetzt – Kasse nicht erreichbar, Parkvorgänge "
                         "nicht zurückgesetzt"};
    }
    std::printf("[reset] %s\n", notice.text.c_str());
    return redirectWithNotice("/betrieb", notice);
}

// Pages and forms only for the logged-in operator; status 0 = not an admin route.
HttpResponse adminRoute(const HttpRequest& req)
{
    bool get = req.method == "GET";
    bool post = req.method == "POST";
    if (get && req.path == "/kasse")
    {
        KasseSnapshot snap = fetchSessions(kasse);
        return htmlPage(renderKassePage(snap.sessions, req.arg("kz"), pendingNotice(req), snap.now, snap.synced,
                                        snap.error));
    }
    if (post && req.path == "/kasse/pay")
    {
        FormResult r = payAtKasse(kasse, req.arg("kz"), req.arg("method"), req.arg("amount"));
        std::string plate = normalizePlate(req.arg("kz"));
        std::printf("[pay] %s: %s\n", plate.c_str(), r.notice.text.c_str());
        return redirectWithNotice("/kasse", r.notice, isValidPlate(plate) ? "kz=" + plate + "&" : "");
    }
    if (get && req.path == "/registrierung")
    {
        return htmlPage(renderRegistrationPage(registrations, pendingNotice(req)));
    }
    if (post && req.path == "/registrierung/add")
    {
        return finishRegistrationForm(handleRegistrationForm(registrations, req.arg("name"), req.arg("kz")));
    }
    if (post && req.path == "/registrierung/delete")
    {
        return finishRegistrationForm(handleDeleteForm(registrations, req.arg("kz")));
    }
    if (get && req.path == "/betrieb")
    {
        OperatorLane entry = operatorLane(entryLane);
        OperatorLane exit = operatorLane(exitLane);
        return htmlPage(renderOperatorPage(entry, exit, eventLog, req.arg("filter"), pendingNotice(req)));
    }
    if (post && req.path == "/betrieb/oeffnen")
    {
        return onRemoteOpen(req);
    }
    if (post && req.path == "/reset")
    {
        return onReset();
    }
    return {0, "", "", "", ""};
}

HttpResponse route(const HttpRequest& req)
{
    bool get = req.method == "GET";
    bool post = req.method == "POST";
    Role role = roleOf(req);
    if (get && req.path == "/")
    {
        return redirect("/kamera");
    }
    if (get && req.path == "/kamera")
    {
        refreshView(entryLane);
        refreshView(exitLane);
        KasseSnapshot snap = fetchSessions(kasse);
        return htmlPage(renderCameraPage(entryLane.view, exitLane.view, snap, registrations, role));
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
    if (post && req.path == "/kamera/durchfahren")
    {
        Lane* lane = laneOf(req);
        if (lane != nullptr)
        {
            showOnTerminal(*lane, driveThrough(lane->terminal, lane->barrier.vehiclePassed()));
        }
        return redirect("/kamera");
    }
    if (post && req.path == "/kamera/schleife")
    {
        Lane* lane = laneOf(req);
        if (lane != nullptr)
        {
            bool up = lane->barrier.state() != BarrierState::Closed;
            lane->loopHeld = req.arg("held") == "1" && up; // no car on the loop under a closed barrier
            std::printf("[loop] %s: %s\n", lane->name, lane->loopHeld ? "belegt" : "frei");
            if (!lane->loopHeld)
            {
                GateResult r = leftLoop(lane->terminal, up);
                if (!r.message.empty())
                {
                    showOnTerminal(*lane, r);
                }
            }
        }
        return redirect("/kamera");
    }
    if (post && req.path == "/kamera/ruftaste")
    {
        Lane* lane = laneOf(req);
        if (lane != nullptr)
        {
            showOnTerminal(*lane, pressCallButton(lane->terminal, nowUtc(), eventLog));
            saveEvents();
        }
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
    if (get && req.path == "/tarif")
    {
        return htmlPage(renderTariffPage(role));
    }
    if (get && req.path == "/login")
    {
        if (role == Role::Admin)
        {
            return redirect(loginTarget(req.arg("next")));
        }
        return htmlPage(renderLoginPage(pendingNotice(req), loginTarget(req.arg("next"))));
    }
    if (post && req.path == "/login")
    {
        return onLogin(req);
    }
    if (post && req.path == "/logout")
    {
        return onLogout(req);
    }
    HttpResponse admin = role == Role::Admin ? adminRoute(req) : HttpResponse{0, "", "", "", ""};
    if (admin.status != 0)
    {
        return admin;
    }
    bool adminPath = req.path == "/kasse" || req.path == "/kasse/pay" || req.path == "/registrierung" ||
                     req.path == "/registrierung/add" || req.path == "/registrierung/delete" ||
                     req.path == "/betrieb" || req.path == "/betrieb/oeffnen" || req.path == "/reset";
    if (adminPath)
    {
        return loginRequired(req);
    }
    return notFound();
}

void updateLane(Lane& lane, uint32_t ms)
{
    BarrierState before = lane.barrier.state();
    BarrierEvent event = lane.barrier.update(ms, lane.loopHeld);
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
    std::printf("  http://localhost:%u/kamera   (lauscht auf %s)\n", SCHRANKEN_PORT, host.c_str());
    std::printf("  Anmelden: http://localhost:%u/login   (Benutzer «betrieb», Passwort aus OPERATOR_PASSWORD)\n",
                SCHRANKEN_PORT);
    std::printf("Beenden mit Ctrl+C\n");
    std::fflush(stdout);
    return runHttpServers(host, {{SCHRANKEN_PORT, route}}, tick) ? 0 : 1;
}
