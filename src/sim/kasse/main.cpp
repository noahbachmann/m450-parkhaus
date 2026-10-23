#include <cstdio>
#include <string>

#include "codec.h"
#include "format.h"
#include "http_server.h"
#include "kasse_api.h"
#include "kasse_pages.h"
#include "kasse_view.h"
#include "plate.h"
#include "sim_common.h"
#include "testdata.h"

// Simulated Pico 1 "Kasse": JSON API for Pico 2, Kasse and Automat pages, sessions in SIM_DATA/sessions.json.
// Same routes as src/kasse/main.cpp.

namespace
{
SessionStore sessions;

// Result of the last POST, shown once after the redirect (post/redirect/get: reload doesn't pay twice).
uint32_t noticeId = 0;
Notice lastNotice;

void saveSessions()
{
    writeDataFile("sessions", sessionsToJson(sessions));
}

HttpResponse api(const ApiResponse& r)
{
    return {r.status, "application/json", r.body, "", ""};
}

HttpResponse redirectWithNotice(const std::string& path, const std::string& plate, const Notice& notice)
{
    lastNotice = notice;
    noticeId++;
    std::string url = path + "?";
    if (isValidPlate(plate))
    {
        url += "kz=" + plate + "&";
    }
    return redirect(url + "m=" + std::to_string(noticeId));
}

HttpResponse showPage(Page p, const HttpRequest& req)
{
    Notice notice;
    if (req.arg("m") == std::to_string(noticeId))
    {
        notice = lastNotice;
    }
    return simPage(renderPage(p, sessions, req.arg("kz"), notice, nowUtc(), true));
}

HttpResponse onPay(Page p, const HttpRequest& req)
{
    std::string plate = normalizePlate(req.arg("kz"));
    FormResult r = handlePayForm(sessions, plate, req.arg("method"), req.arg("amount"), nowUtc(), true);
    if (r.changed)
    {
        saveSessions();
    }
    std::printf("[pay] %s: %s\n", plate.c_str(), r.notice.text.c_str());
    return redirectWithNotice(pagePath(p), plate, r.notice);
}

HttpResponse route(const HttpRequest& req)
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
        return showPage(Page::Kasse, req);
    }
    if (get && req.path == "/automat")
    {
        return showPage(Page::Automat, req);
    }
    if (post && req.path == "/kasse/pay")
    {
        return onPay(Page::Kasse, req);
    }
    if (post && req.path == "/automat/pay")
    {
        return onPay(Page::Automat, req);
    }
    if (post && req.path == "/reset")
    {
        loadDefaultSessions(sessions);
        saveSessions();
        return redirectWithNotice("/kasse", "",
                                  {"ok", "Testdaten zurückgesetzt – keine Parkvorgänge, Transaktionszähler auf 1"});
    }
    return notFound();
}

void loadSessions()
{
    makeDataDir();
    std::string json;
    if (!readDataFile("sessions", json) || !sessionsFromJson(json, sessions))
    {
        loadDefaultSessions(sessions);
        saveSessions();
        std::printf("sessions: defaults written\n");
        return;
    }
    std::printf("sessions: loaded %u\n", static_cast<unsigned>(sessions.all().size()));
}
}

int main()
{
    loadSessions();
    std::string host = listenHost();
    std::printf("Simulator Pico 1 «Kasse», %s, Daten in %s/\n", formatDateTime(nowUtc()).c_str(), dataDir().c_str());
    std::printf("  http://localhost:%u/kasse   http://localhost:%u/automat   (lauscht auf %s)\n", KASSE_PORT,
                KASSE_PORT, host.c_str());
    std::printf("Beenden mit Ctrl+C\n");
    std::fflush(stdout);
    return runHttpServers(host, {{KASSE_PORT, route}}, []() { std::fflush(stdout); }) ? 0 : 1;
}
