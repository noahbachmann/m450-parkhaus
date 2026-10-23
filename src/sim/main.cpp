#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#include "barrier.h"
#include "codec.h"
#include "format.h"
#include "gate.h"
#include "html.h"
#include "http_server.h"
#include "kasse_api.h"
#include "kasse_pages.h"
#include "kasse_view.h"
#include "plate.h"
#include "schranken_view.h"
#include "testdata.h"

// PC simulator of both Picos: same lib/ code as the firmware, served on localhost.
//   Pico 1 "Kasse":     http://localhost:8081  (/kasse, /automat, /api/...)
//   Pico 2 "Schranken": http://localhost:8082  (/kamera, /registrierung, /sim = loops + Kasse on/off)
// Data is kept in sim-data/*.json, same format as LittleFS on the Picos.

namespace
{
constexpr uint16_t KASSE_PORT = 8081;
constexpr uint16_t SCHRANKEN_PORT = 8082;
const char* DATA_DIR = "sim-data";

int64_t nowUtc()
{
    return static_cast<int64_t>(std::time(nullptr));
}

uint32_t nowMs()
{
    using namespace std::chrono;
    return static_cast<uint32_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

std::string dataPath(const char* name)
{
    return std::string(DATA_DIR) + "/" + name + ".json";
}

bool readFile(const char* name, std::string& out)
{
    std::ifstream in(dataPath(name), std::ios::binary);
    if (!in)
    {
        return false;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

void writeFile(const char* name, const std::string& data)
{
    std::ofstream out(dataPath(name), std::ios::binary | std::ios::trunc);
    out << data;
    if (!out)
    {
        std::printf("%s: write FAILED\n", dataPath(name).c_str());
    }
}

void makeDataDir()
{
#ifdef _WIN32
    _mkdir(DATA_DIR);
#else
    mkdir(DATA_DIR, 0755);
#endif
}

HttpResponse redirect(const std::string& location)
{
    HttpResponse r;
    r.status = 303;
    r.location = location;
    return r;
}

HttpResponse notFound()
{
    return {404, "text/plain; charset=utf-8", "Nicht gefunden", ""};
}

// Simulator links above every page, so both "Picos" are one click apart.
HttpResponse page(const std::string& html)
{
    std::string banner = "<div style=\"background:#1d232b;color:#fff;padding:8px 12px;border-radius:6px;"
                         "margin-bottom:12px;font-size:.9rem\"><strong>PC-Simulator</strong> · "
                         "<a style=\"color:#9cf\" href=\"http://localhost:" + std::to_string(KASSE_PORT) +
                         "/kasse\">Kasse</a> · <a style=\"color:#9cf\" href=\"http://localhost:" +
                         std::to_string(KASSE_PORT) + "/automat\">Automat</a> · "
                         "<a style=\"color:#9cf\" href=\"http://localhost:" + std::to_string(SCHRANKEN_PORT) +
                         "/kamera\">Kamera</a> · <a style=\"color:#9cf\" href=\"http://localhost:" +
                         std::to_string(SCHRANKEN_PORT) + "/registrierung\">Registrierung</a> · "
                         "<a style=\"color:#9cf\" href=\"http://localhost:" + std::to_string(SCHRANKEN_PORT) +
                         "/sim\">Steuerung (Schleifen, Kasse)</a></div>";
    std::string out = html;
    size_t body = out.find("<body>");
    if (body != std::string::npos)
    {
        out.insert(body + 6, banner);
    }
    return {200, "text/html; charset=utf-8", out, ""};
}

// ---- Pico 1 "Kasse" ----

SessionStore sessions;
uint32_t kasseNoticeId = 0;
Notice kasseNotice;

void saveSessions()
{
    writeFile("sessions", sessionsToJson(sessions));
}

HttpResponse api(const ApiResponse& r)
{
    return {r.status, "application/json", r.body, ""};
}

HttpResponse kasseRedirect(const std::string& path, const std::string& plate, const Notice& notice)
{
    kasseNotice = notice;
    kasseNoticeId++;
    std::string url = path + "?";
    if (isValidPlate(plate))
    {
        url += "kz=" + plate + "&";
    }
    return redirect(url + "m=" + std::to_string(kasseNoticeId));
}

HttpResponse showKassePage(Page p, const HttpRequest& req)
{
    Notice notice;
    if (req.arg("m") == std::to_string(kasseNoticeId))
    {
        notice = kasseNotice;
    }
    return page(renderPage(p, sessions, req.arg("kz"), notice, nowUtc(), true));
}

HttpResponse payForm(Page p, const HttpRequest& req)
{
    std::string plate = normalizePlate(req.arg("kz"));
    FormResult r = handlePayForm(sessions, plate, req.arg("method"), req.arg("amount"), nowUtc(), true);
    if (r.changed)
    {
        saveSessions();
    }
    std::printf("[pay] %s: %s\n", plate.c_str(), r.notice.text.c_str());
    return kasseRedirect(pagePath(p), plate, r.notice);
}

HttpResponse kasse(const HttpRequest& req)
{
    bool get = req.method == "GET";
    bool post = req.method == "POST";
    if (get && req.path == "/api/status")
    {
        return api(handleStatus(sessions, req.arg("kz"), nowUtc()));
    }
    if (post && req.path == "/api/entry")
    {
        ApiResponse r = handleEntry(sessions, req.body, nowUtc(), true);
        if (r.status == 201)
        {
            saveSessions();
        }
        return api(r);
    }
    if (post && req.path == "/api/exit")
    {
        ApiResponse r = handleExit(sessions, req.body, nowUtc(), true);
        if (r.status == 200)
        {
            saveSessions();
        }
        return api(r);
    }
    if (get && req.path == "/")
    {
        return redirect("/kasse");
    }
    if (get && req.path == "/kasse")
    {
        return showKassePage(Page::Kasse, req);
    }
    if (get && req.path == "/automat")
    {
        return showKassePage(Page::Automat, req);
    }
    if (post && req.path == "/kasse/pay")
    {
        return payForm(Page::Kasse, req);
    }
    if (post && req.path == "/automat/pay")
    {
        return payForm(Page::Automat, req);
    }
    if (post && req.path == "/reset")
    {
        loadDefaultSessions(sessions);
        saveSessions();
        return kasseRedirect("/kasse", "",
                             {"ok", "Testdaten zurückgesetzt – keine Parkvorgänge, Transaktionszähler auf 1"});
    }
    return notFound();
}

// ---- Pico 2 "Schranken" ----

bool kasseOnline = true;

// Pico 2 -> Pico 1 through the Kasse routing above, as if over HTTP. Offline = no answer (timeout).
class SimKasseLink : public KasseLink
{
public:
    HttpReply postJson(const char* path, const std::string& body) override
    {
        if (!kasseOnline)
        {
            return {};
        }
        HttpRequest req;
        req.method = "POST";
        req.path = path;
        req.body = body;
        HttpResponse res = kasse(req);
        std::printf("[link] POST %s %s -> %d %s\n", path, body.c_str(), res.status, res.body.c_str());
        return {res.status, res.body};
    }
};

struct Lane
{
    const char* name;
    Barrier barrier;
    bool loopOccupied = false;
    LaneView view;
};

SimKasseLink link;
RegistrationList registrations;
Lane entryLane{"Einfahrt", {}, false, {}};
Lane exitLane{"Ausfahrt", {}, false, {}};
uint32_t registrationNoticeId = 0;
Notice registrationNotice;

void saveRegistrations()
{
    writeFile("registrations", registrationsToJson(registrations));
}

void applyGate(Lane& lane, const GateResult& r)
{
    if (r.open)
    {
        lane.barrier.open(nowMs());
    }
    lane.view.cls = r.cls;
    lane.view.terminal = r.message;
    std::printf("[gate] %s %s: %s\n", lane.name, r.plate.c_str(), r.message.c_str());
}

void refreshView(Lane& lane)
{
    lane.view.barrier = barrierStateLabel(lane.barrier.state());
    lane.view.fault = lane.barrier.loopFault() ? loopFaultMessage(lane.name) : "";
}

HttpResponse registrationRedirect(const FormResult& r)
{
    if (r.changed)
    {
        saveRegistrations();
    }
    std::printf("[registration] %s\n", r.notice.text.c_str());
    registrationNotice = r.notice;
    registrationNoticeId++;
    return redirect("/registrierung?m=" + std::to_string(registrationNoticeId));
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
    appendPageStart(html, "Simulator", "<a href=\"/kamera\">Kamerasimulation</a><a href=\"/registrierung\">Registrierung</a>",
                    "Simulator-Steuerung");
    html += "<p class=\"muted\">Ersetzt die Hardware: Taster der Sicherungsschleifen und Strom von Pico 1.</p>"
            "<div class=\"card\"><table>";
    appendLoopControl(html, "entryLoop", entryLane);
    appendLoopControl(html, "exitLoop", exitLane);
    html += "<tr><th>Pico 1 «Kasse»</th><td>";
    html += kasseOnline ? "erreichbar" : "<strong>aus</strong>";
    html += "</td><td><form method=\"post\" action=\"/sim/set\" class=\"inline\"><input type=\"hidden\" name=\"kasse\" "
            "value=\"";
    html += kasseOnline ? "0" : "1";
    html += "\"><button type=\"submit\">";
    html += kasseOnline ? "ausschalten" : "einschalten";
    html += "</button></form></td><td></td></tr></table></div>"
            "<p><a href=\"/sim\">Aktualisieren</a></p>";
    appendPageEnd(html);
    return page(html);
}

HttpResponse simSet(const HttpRequest& req)
{
    if (req.hasArg("entryLoop"))
    {
        entryLane.loopOccupied = req.arg("entryLoop") == "1";
    }
    if (req.hasArg("exitLoop"))
    {
        exitLane.loopOccupied = req.arg("exitLoop") == "1";
    }
    if (req.hasArg("kasse"))
    {
        kasseOnline = req.arg("kasse") == "1";
        std::printf("[sim] Kasse %s\n", kasseOnline ? "on" : "off");
    }
    return redirect("/sim");
}

HttpResponse schranken(const HttpRequest& req)
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
        return page(renderCameraPage(entryLane.view, exitLane.view));
    }
    if (post && req.path == "/kamera/einfahrt")
    {
        applyGate(entryLane, entryRequest(registrations, req.arg("kz"), req.arg("entry"), link));
        return redirect("/kamera");
    }
    if (post && req.path == "/kamera/ausfahrt")
    {
        applyGate(exitLane, exitRequest(req.arg("kz"), link));
        return redirect("/kamera");
    }
    if (get && req.path == "/registrierung")
    {
        Notice notice;
        if (req.arg("m") == std::to_string(registrationNoticeId))
        {
            notice = registrationNotice;
        }
        return page(renderRegistrationPage(registrations, notice));
    }
    if (post && req.path == "/registrierung/add")
    {
        return registrationRedirect(handleRegistrationForm(registrations, req.arg("name"), req.arg("kz")));
    }
    if (post && req.path == "/registrierung/delete")
    {
        return registrationRedirect(handleDeleteForm(registrations, req.arg("kz")));
    }
    if (post && req.path == "/reset")
    {
        loadDefaultRegistrations(registrations);
        saveRegistrations();
        return registrationRedirect({{"ok", "Testdaten zurückgesetzt – Registrierungen KZ-01, KZ-02, KZ-08, KZ-10"}, false});
    }
    if (get && req.path == "/sim")
    {
        return simControl();
    }
    if (post && req.path == "/sim/set")
    {
        return simSet(req);
    }
    return notFound();
}

void updateLane(Lane& lane, uint32_t ms)
{
    BarrierState before = lane.barrier.state();
    BarrierEvent event = lane.barrier.update(ms, lane.loopOccupied);
    if (event == BarrierEvent::SafetyStop)
    {
        std::printf("[safety] Sicherheitsabschaltung %s\n", lane.name);
    }
    else if (event == BarrierEvent::LoopFault)
    {
        std::printf("[safety] %s\n", loopFaultMessage(lane.name).c_str());
    }
    if (lane.barrier.state() != before)
    {
        std::printf("[barrier] %s: %s\n", lane.name, barrierStateLabel(lane.barrier.state()));
    }
    std::fflush(stdout);
}

void tick()
{
    uint32_t ms = nowMs();
    updateLane(entryLane, ms);
    updateLane(exitLane, ms);
}

void loadData()
{
    makeDataDir();
    std::string json;
    if (!readFile("sessions", json) || !sessionsFromJson(json, sessions))
    {
        loadDefaultSessions(sessions);
        saveSessions();
        std::printf("sessions: defaults written\n");
    }
    if (!readFile("registrations", json) || !registrationsFromJson(json, registrations))
    {
        loadDefaultRegistrations(registrations);
        saveRegistrations();
        std::printf("registrations: defaults written\n");
    }
    std::printf("sessions: %u, registrations: %u (in %s/)\n", static_cast<unsigned>(sessions.all().size()),
                static_cast<unsigned>(registrations.all().size()), DATA_DIR);
}
}

int main()
{
    loadData();
    std::printf("Parkhaus-Simulator, %s\n", formatDateTime(nowUtc()).c_str());
    std::printf("  Kasse:      http://localhost:%u/kasse   http://localhost:%u/automat\n", KASSE_PORT, KASSE_PORT);
    std::printf("  Schranken:  http://localhost:%u/kamera  http://localhost:%u/registrierung\n", SCHRANKEN_PORT,
                SCHRANKEN_PORT);
    std::printf("  Steuerung:  http://localhost:%u/sim  (Sicherungsschleifen, Kasse an/aus)\n", SCHRANKEN_PORT);
    std::printf("Beenden mit Ctrl+C\n");
    std::fflush(stdout);
    return runHttpServers({{KASSE_PORT, kasse}, {SCHRANKEN_PORT, schranken}}, tick) ? 0 : 1;
}
