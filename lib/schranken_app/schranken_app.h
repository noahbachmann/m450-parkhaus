#pragma once

#include <stdint.h>
#include <string>
#include <vector>

#include "auth.h"
#include "barrier.h"
#include "event_log.h"
#include "gate.h"
#include "html.h"
#include "operator_view.h"
#include "registration.h"
#include "schranken_view.h"
#include "terminal.h"
#include "web.h"

// Pico 2 "Schranken" as one platform-neutral app: every route, the login, the one-time notices after a redirect
// and the decisions what to save. The board (src/schranken) and the PC simulator (src/sim/schranken) only adapt
// their web server, storage, clock and barriers to it, so both run exactly the same routes.

// One barrier as the app sees it. Board: the barrier runs on core 1; simulator and tests: LocalBarrier.
class BarrierPort
{
public:
    virtual ~BarrierPort() = default;
    virtual BarrierState state() const = 0;
    virtual bool loopFault() const = 0;
    virtual bool loopHeld() const = 0;
    virtual void requestOpen() = 0;
    // «Durchfahren»: the result of Barrier::vehiclePassed() (false = barrier closed).
    virtual bool requestPass() = 0;
    // «Fahrzeug bleibt stehen» from the web page, ORed with the real loop input.
    virtual void setLoopHeld(bool held) = 0;
};

class AppPlatform
{
public:
    virtual ~AppPlatform() = default;
    // Files "registrations" and "events" (JSON). Writes must be atomic; the platform reports failures itself.
    virtual bool readFile(const char* name, std::string& json) = 0;
    virtual void writeFile(const char* name, const std::string& json) = 0;
    // For login tokens.
    virtual uint32_t random32() = 0;
    // One line for the console, e.g. "[gate] Einfahrt LU123456: LU123456 – Willkommen, Einfahrt frei".
    virtual void log(const std::string& line) = 0;
};

class SchrankenApp
{
public:
    SchrankenApp(KasseLink& kasse, BarrierPort& entry, BarrierPort& exit, AppPlatform& platform,
                 std::string operatorPassword);

    // Loads registrations and event log; missing or invalid files are replaced by the defaults / an empty log.
    void begin();
    // now: Unix epoch seconds (UTC).
    WebResponse handle(const WebRequest& req, int64_t now);
    // A barrier reported safety stops or loop faults: logged, then saved once. None is ignored.
    void onBarrierEvents(GateLane lane, const std::vector<BarrierEvent>& events, int64_t now);
    void onBarrierEvent(GateLane lane, BarrierEvent event, int64_t now);

    const RegistrationList& registrations() const;
    const EventLog& events() const;
    const LaneTerminal& terminal(GateLane lane) const;

    // The route needs the operator login.
    static bool isAdminRoute(const std::string& method, const std::string& path);
    // Where to go after the login: next if it is an admin page (GET), else /betrieb («next» comes from the URL).
    static std::string loginTarget(const std::string& next);

private:
    struct Lane
    {
        GateLane id;
        BarrierPort& barrier;
        LaneTerminal terminal;
        LaneView view;
    };
    struct Ctx
    {
        int64_t now;
        Role role;
    };
    using Handler = WebResponse (SchrankenApp::*)(const WebRequest&, const Ctx&);
    struct Route
    {
        const char* method;
        const char* path;
        bool admin;
        Handler handler;
    };
    static const Route ROUTES[];

    WebResponse home(const WebRequest& req, const Ctx& ctx);
    WebResponse camera(const WebRequest& req, const Ctx& ctx);
    WebResponse entryGate(const WebRequest& req, const Ctx& ctx);
    WebResponse exitGate(const WebRequest& req, const Ctx& ctx);
    WebResponse callButton(const WebRequest& req, const Ctx& ctx);
    WebResponse driveThrough(const WebRequest& req, const Ctx& ctx);
    WebResponse loop(const WebRequest& req, const Ctx& ctx);
    WebResponse abortExit(const WebRequest& req, const Ctx& ctx);
    WebResponse terminalPay(const WebRequest& req, const Ctx& ctx);
    WebResponse automatPage(const WebRequest& req, const Ctx& ctx);
    WebResponse automatPay(const WebRequest& req, const Ctx& ctx);
    WebResponse tariff(const WebRequest& req, const Ctx& ctx);
    WebResponse loginPage(const WebRequest& req, const Ctx& ctx);
    WebResponse login(const WebRequest& req, const Ctx& ctx);
    WebResponse logout(const WebRequest& req, const Ctx& ctx);
    WebResponse kassePage(const WebRequest& req, const Ctx& ctx);
    WebResponse kassePay(const WebRequest& req, const Ctx& ctx);
    WebResponse registrationPage(const WebRequest& req, const Ctx& ctx);
    WebResponse registrationAdd(const WebRequest& req, const Ctx& ctx);
    WebResponse registrationDelete(const WebRequest& req, const Ctx& ctx);
    WebResponse operatorPage(const WebRequest& req, const Ctx& ctx);
    WebResponse remoteOpening(const WebRequest& req, const Ctx& ctx);
    WebResponse reset(const WebRequest& req, const Ctx& ctx);

    Lane& lane(GateLane id);
    Lane* laneArg(const WebRequest& req);
    // Shows a terminal result (gate decision, payment, call, abort), opens the barrier if it says so and logs
    // «Kasse nicht erreichbar».
    void show(Lane& lane, const GateResult& r, int64_t now);
    void refresh(Lane& lane, int64_t now);
    OperatorLane operatorLane(Lane& lane);
    Notice pendingNotice(const WebRequest& req) const;
    // query: extra arguments, e.g. "kz=LU123456&".
    WebResponse redirectWithNotice(const std::string& path, const Notice& notice, const std::string& query = "");
    // Kasse and Automat: POST /api/pay, then back to page with the result and the plate.
    WebResponse payAndShow(const std::string& page, const WebRequest& req);
    WebResponse registrationDone(const FormResult& r);
    void saveIfChanged(uint32_t registrationsBefore, uint32_t eventsBefore);

    KasseLink& kasse;
    AppPlatform& platform;
    std::string password;
    Lane entryLane;
    Lane exitLane;
    RegistrationList registrationList;
    EventLog eventLog;
    LoginSessions logins;
    // Result of the last POST, shown once after the redirect (?m=<noticeId>).
    uint32_t noticeId = 0;
    Notice lastNotice;
};
