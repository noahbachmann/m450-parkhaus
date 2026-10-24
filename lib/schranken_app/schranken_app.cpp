#include "schranken_app.h"

#include "codec.h"
#include "format.h"
#include "kasse_client.h"
#include "kasse_pages.h"
#include "operator_view.h"
#include "plate.h"
#include "testdata.h"

const SchrankenApp::Route SchrankenApp::ROUTES[] = {
    {"GET", "/", false, &SchrankenApp::home},
    {"GET", "/kamera", false, &SchrankenApp::camera},
    {"POST", "/kamera/einfahrt", false, &SchrankenApp::entryGate},
    {"POST", "/kamera/ausfahrt", false, &SchrankenApp::exitGate},
    {"POST", "/kamera/ruftaste", false, &SchrankenApp::callButton},
    {"POST", "/kamera/durchfahren", false, &SchrankenApp::driveThrough},
    {"POST", "/kamera/schleife", false, &SchrankenApp::loop},
    {"POST", "/kamera/abbrechen", false, &SchrankenApp::abortExit},
    {"POST", "/kamera/bezahlen", false, &SchrankenApp::terminalPay},
    {"GET", "/automat", false, &SchrankenApp::automatPage},
    {"POST", "/automat/pay", false, &SchrankenApp::automatPay},
    {"GET", "/tarif", false, &SchrankenApp::tariff},
    {"GET", "/login", false, &SchrankenApp::loginPage},
    {"POST", "/login", false, &SchrankenApp::login},
    {"POST", "/logout", false, &SchrankenApp::logout},
    {"GET", "/kasse", true, &SchrankenApp::kassePage},
    {"POST", "/kasse/pay", true, &SchrankenApp::kassePay},
    {"GET", "/registrierung", true, &SchrankenApp::registrationPage},
    {"POST", "/registrierung/add", true, &SchrankenApp::registrationAdd},
    {"POST", "/registrierung/delete", true, &SchrankenApp::registrationDelete},
    {"GET", "/betrieb", true, &SchrankenApp::operatorPage},
    {"POST", "/betrieb/oeffnen", true, &SchrankenApp::remoteOpening},
    {"POST", "/reset", true, &SchrankenApp::reset},
};

SchrankenApp::SchrankenApp(KasseLink& kasseLink, BarrierPort& entry, BarrierPort& exit, AppPlatform& appPlatform,
                           std::string operatorPassword)
    : kasse(kasseLink), platform(appPlatform), password(std::move(operatorPassword)),
      entryLane{GateLane::Entry, entry, {}, {}}, exitLane{GateLane::Exit, exit, {}, {}}
{
    entryLane.terminal.lane = GateLane::Entry;
    exitLane.terminal.lane = GateLane::Exit;
}

void SchrankenApp::begin()
{
    std::string json;
    if (!platform.readFile("registrations", json) || !registrationsFromJson(json, registrationList))
    {
        loadDefaultRegistrations(registrationList);
        platform.writeFile("registrations", registrationsToJson(registrationList));
        platform.log("registrations: defaults written");
    }
    else
    {
        platform.log("registrations: loaded " + std::to_string(registrationList.all().size()));
    }
    if (!platform.readFile("events", json) || !eventsFromJson(json, eventLog))
    {
        eventLog.clear();
        platform.writeFile("events", eventsToJson(eventLog));
        platform.log("events: empty log written");
    }
    else
    {
        platform.log("events: loaded " + std::to_string(eventLog.all().size()));
    }
}

WebResponse SchrankenApp::handle(const WebRequest& req, int64_t now)
{
    Ctx ctx{now, logins.valid(req.cookie) ? Role::Admin : Role::Public};
    for (const Route& route : ROUTES)
    {
        if (req.method != route.method || req.path != route.path)
        {
            continue;
        }
        if (route.admin && ctx.role != Role::Admin)
        {
            return redirect("/login?next=" + loginTarget(req.path));
        }
        uint32_t registrationsBefore = registrationList.revision();
        uint32_t eventsBefore = eventLog.revision();
        WebResponse res = (this->*route.handler)(req, ctx);
        saveIfChanged(registrationsBefore, eventsBefore);
        return res;
    }
    return notFound();
}

void SchrankenApp::onBarrierEvents(GateLane laneId, const std::vector<BarrierEvent>& events, int64_t now)
{
    uint32_t eventsBefore = eventLog.revision();
    for (BarrierEvent event : events)
    {
        if (logBarrierEvent(eventLog, laneId, event, now))
        {
            platform.log(std::string("[safety] ") + eventTypeLabel(eventLog.all().back().type) + " " +
                         gateLaneLabel(laneId));
        }
    }
    saveIfChanged(registrationList.revision(), eventsBefore);
}

void SchrankenApp::onBarrierEvent(GateLane laneId, BarrierEvent event, int64_t now)
{
    onBarrierEvents(laneId, {event}, now);
}

const RegistrationList& SchrankenApp::registrations() const
{
    return registrationList;
}

const EventLog& SchrankenApp::events() const
{
    return eventLog;
}

const LaneTerminal& SchrankenApp::terminal(GateLane id) const
{
    return id == GateLane::Exit ? exitLane.terminal : entryLane.terminal;
}

bool SchrankenApp::isAdminRoute(const std::string& method, const std::string& path)
{
    for (const Route& route : ROUTES)
    {
        if (method == route.method && path == route.path)
        {
            return route.admin;
        }
    }
    return false;
}

std::string SchrankenApp::loginTarget(const std::string& next)
{
    for (const Route& route : ROUTES)
    {
        if (route.admin && std::string(route.method) == "GET" && next == route.path)
        {
            return next;
        }
    }
    return "/betrieb";
}

WebResponse SchrankenApp::home(const WebRequest&, const Ctx&)
{
    return redirect("/kamera");
}

WebResponse SchrankenApp::camera(const WebRequest&, const Ctx& ctx)
{
    refresh(entryLane, ctx.now);
    refresh(exitLane, ctx.now);
    KasseSnapshot snap = fetchSessions(kasse);
    return htmlPage(renderCameraPage(entryLane.view, exitLane.view, snap, registrationList, ctx.role));
}

WebResponse SchrankenApp::entryGate(const WebRequest& req, const Ctx& ctx)
{
    GateResult r = entryRequest(registrationList, req.arg("kz"), req.arg("entry"), kasse);
    recordGate(entryLane.terminal, r);
    show(entryLane, r, ctx.now);
    return redirect("/kamera");
}

WebResponse SchrankenApp::exitGate(const WebRequest& req, const Ctx& ctx)
{
    GateResult r = exitRequest(req.arg("kz"), kasse);
    recordGate(exitLane.terminal, r);
    show(exitLane, r, ctx.now);
    return redirect("/kamera");
}

WebResponse SchrankenApp::callButton(const WebRequest& req, const Ctx& ctx)
{
    if (Lane* l = laneArg(req))
    {
        show(*l, pressCallButton(l->terminal, ctx.now, eventLog), ctx.now);
    }
    return redirect("/kamera");
}

WebResponse SchrankenApp::driveThrough(const WebRequest& req, const Ctx& ctx)
{
    if (Lane* l = laneArg(req))
    {
        show(*l, ::driveThrough(l->terminal, l->barrier.requestPass()), ctx.now);
    }
    return redirect("/kamera");
}

WebResponse SchrankenApp::loop(const WebRequest& req, const Ctx& ctx)
{
    if (Lane* l = laneArg(req))
    {
        bool up = l->barrier.state() != BarrierState::Closed;
        bool held = req.arg("held") == "1" && up; // no car on the loop under a closed barrier
        l->barrier.setLoopHeld(held);
        platform.log(std::string("[loop] ") + gateLaneLabel(l->id) + ": " + (held ? "belegt" : "frei"));
        if (!held)
        {
            GateResult r = leftLoop(l->terminal, up);
            if (!r.message.empty())
            {
                show(*l, r, ctx.now);
            }
        }
    }
    return redirect("/kamera");
}

WebResponse SchrankenApp::abortExit(const WebRequest&, const Ctx& ctx)
{
    show(exitLane, abortAtTerminal(exitLane.terminal), ctx.now);
    return redirect("/kamera");
}

WebResponse SchrankenApp::terminalPay(const WebRequest& req, const Ctx& ctx)
{
    show(exitLane, payAtTerminal(exitLane.terminal, req.arg("method"), ctx.now, kasse, eventLog), ctx.now);
    return redirect("/kamera");
}

WebResponse SchrankenApp::automatPage(const WebRequest& req, const Ctx& ctx)
{
    KasseSnapshot snap = fetchSessions(kasse);
    return htmlPage(renderAutomatPage(snap.sessions, req.arg("kz"), pendingNotice(req), snap.now, snap.synced,
                                      snap.error, ctx.role));
}

WebResponse SchrankenApp::automatPay(const WebRequest& req, const Ctx&)
{
    return payAndShow("/automat", req);
}

WebResponse SchrankenApp::tariff(const WebRequest&, const Ctx& ctx)
{
    return htmlPage(renderTariffPage(ctx.role));
}

WebResponse SchrankenApp::loginPage(const WebRequest& req, const Ctx& ctx)
{
    std::string next = loginTarget(req.arg("next"));
    if (ctx.role == Role::Admin)
    {
        return redirect(next);
    }
    return htmlPage(renderLoginPage(pendingNotice(req), next));
}

WebResponse SchrankenApp::login(const WebRequest& req, const Ctx&)
{
    std::string next = loginTarget(req.arg("next"));
    if (!credentialsOk(req.arg("user"), req.arg("password"), password))
    {
        platform.log("[login] rejected");
        return redirectWithNotice("/login", {"err", "Benutzer oder Passwort falsch"}, "next=" + next + "&");
    }
    uint32_t words[4];
    for (uint32_t& w : words)
    {
        w = platform.random32();
    }
    std::string token = makeToken(words);
    logins.add(token);
    platform.log(std::string("[login] ") + OPERATOR_USER);
    WebResponse res = redirect(next);
    res.setCookie = loginCookie(token);
    return res;
}

WebResponse SchrankenApp::logout(const WebRequest& req, const Ctx&)
{
    logins.remove(cookieValue(req.cookie, SESSION_COOKIE));
    WebResponse res = redirect("/kamera");
    res.setCookie = logoutCookie();
    return res;
}

WebResponse SchrankenApp::kassePage(const WebRequest& req, const Ctx&)
{
    KasseSnapshot snap = fetchSessions(kasse);
    return htmlPage(
        renderKassePage(snap.sessions, req.arg("kz"), pendingNotice(req), snap.now, snap.synced, snap.error));
}

WebResponse SchrankenApp::kassePay(const WebRequest& req, const Ctx&)
{
    return payAndShow("/kasse", req);
}

WebResponse SchrankenApp::registrationPage(const WebRequest& req, const Ctx&)
{
    return htmlPage(renderRegistrationPage(registrationList, pendingNotice(req)));
}

WebResponse SchrankenApp::registrationAdd(const WebRequest& req, const Ctx&)
{
    return registrationDone(handleRegistrationForm(registrationList, req.arg("name"), req.arg("kz")));
}

WebResponse SchrankenApp::registrationDelete(const WebRequest& req, const Ctx&)
{
    return registrationDone(handleDeleteForm(registrationList, req.arg("kz")));
}

WebResponse SchrankenApp::operatorPage(const WebRequest& req, const Ctx&)
{
    OperatorLane entry = operatorLane(entryLane);
    OperatorLane exit = operatorLane(exitLane);
    return htmlPage(renderOperatorPage(entry, exit, eventLog, req.arg("filter"), pendingNotice(req)));
}

WebResponse SchrankenApp::remoteOpening(const WebRequest& req, const Ctx& ctx)
{
    GateLane id = GateLane::None;
    gateLaneFromName(req.arg("lane"), id);
    RemoteOpenResult r = remoteOpen(id, req.arg("kz"), req.arg("reason"), req.arg("name"), ctx.now, kasse,
                                    registrationList, eventLog);
    if (r.open)
    {
        Lane& l = lane(id);
        l.barrier.requestOpen();
        recordRemoteOpening(l.terminal, r.plate);
    }
    platform.log("[betrieb] " + r.notice.text);
    return redirectWithNotice("/betrieb", r.notice);
}

WebResponse SchrankenApp::reset(const WebRequest&, const Ctx&)
{
    loadDefaultRegistrations(registrationList);
    eventLog.clear();
    Notice notice{"ok", "Testdaten zurückgesetzt – Registrierungen KZ-01, KZ-02, KZ-08, KZ-10, Ereignisprotokoll leer, "
                        "keine Parkvorgänge"};
    if (!resetKasse(kasse))
    {
        notice = {"err", "Registrierungen und Ereignisprotokoll zurückgesetzt – Kasse nicht erreichbar, Parkvorgänge "
                         "nicht zurückgesetzt"};
    }
    platform.log("[reset] " + notice.text);
    return redirectWithNotice("/betrieb", notice);
}

SchrankenApp::Lane& SchrankenApp::lane(GateLane id)
{
    return id == GateLane::Exit ? exitLane : entryLane;
}

SchrankenApp::Lane* SchrankenApp::laneArg(const WebRequest& req)
{
    GateLane id = GateLane::None;
    gateLaneFromName(req.arg("lane"), id);
    return id == GateLane::None ? nullptr : &lane(id);
}

void SchrankenApp::show(Lane& l, const GateResult& r, int64_t now)
{
    if (r.open)
    {
        l.barrier.requestOpen();
    }
    logGateResult(eventLog, l.id, r, now);
    l.view.cls = r.cls;
    l.view.terminal = r.message;
    platform.log(std::string("[gate] ") + gateLaneLabel(l.id) + " " + r.plate + ": " + r.message);
}

void SchrankenApp::refresh(Lane& l, int64_t now)
{
    BarrierState state = l.barrier.state();
    if (state == BarrierState::Closed && l.terminal.passing)
    {
        show(l, closedAfterPass(l.terminal), now);
    }
    bool held = l.barrier.loopHeld();
    l.view.canPass = state == BarrierState::Open && !l.terminal.passing && !held;
    l.view.canHold = state != BarrierState::Closed && !held;
    l.view.loopHeld = held;
    l.view.barrier = barrierStateLabel(state);
    l.view.fault = l.barrier.loopFault() ? loopFaultMessage(gateLaneLabel(l.id)) : "";
    l.view.payPlate = l.terminal.payPlate;
    l.view.payFee = l.terminal.payPlate.empty() ? "" : formatChf(l.terminal.payFeeRappen);
}

OperatorLane SchrankenApp::operatorLane(Lane& l)
{
    OperatorLane o;
    o.lane = l.id;
    o.barrier = barrierStateLabel(l.barrier.state());
    o.lastPlate = l.terminal.lastPlate;
    o.status = laneStatus(kasse, l.terminal.lastPlate);
    o.fault = l.barrier.loopFault() ? loopFaultMessage(gateLaneLabel(l.id)) : "";
    o.call = callLabel(l.terminal);
    return o;
}

Notice SchrankenApp::pendingNotice(const WebRequest& req) const
{
    return req.arg("m") == std::to_string(noticeId) ? lastNotice : Notice{};
}

WebResponse SchrankenApp::redirectWithNotice(const std::string& path, const Notice& notice, const std::string& query)
{
    lastNotice = notice;
    noticeId++;
    return redirect(path + "?" + query + "m=" + std::to_string(noticeId));
}

WebResponse SchrankenApp::payAndShow(const std::string& page, const WebRequest& req)
{
    FormResult r = payAtKasse(kasse, req.arg("kz"), req.arg("method"), req.arg("amount"));
    std::string plate = normalizePlate(req.arg("kz"));
    platform.log("[pay] " + page + " " + plate + ": " + r.notice.text);
    return redirectWithNotice(page, r.notice, isValidPlate(plate) ? "kz=" + plate + "&" : "");
}

WebResponse SchrankenApp::registrationDone(const FormResult& r)
{
    platform.log("[registration] " + r.notice.text);
    return redirectWithNotice("/registrierung", r.notice);
}

void SchrankenApp::saveIfChanged(uint32_t registrationsBefore, uint32_t eventsBefore)
{
    if (registrationList.revision() != registrationsBefore)
    {
        platform.writeFile("registrations", registrationsToJson(registrationList));
    }
    if (eventLog.revision() != eventsBefore)
    {
        platform.writeFile("events", eventsToJson(eventLog));
    }
}
