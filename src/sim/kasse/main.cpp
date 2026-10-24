#include <cstdio>
#include <string>

#include "codec.h"
#include "format.h"
#include "http_server.h"
#include "kasse_api.h"
#include "kasse_pages.h"
#include "sim_common.h"
#include "testdata.h"

// Simulated Pico 1 "Kasse": JSON API for Pico 2, sessions in SIM_DATA/sessions.json. The pages (Kasse, Tarif)
// are served by Pico 2, GET / links there (SCHRANKEN_PUBLIC_URL). Same routes as src/kasse/main.cpp.

namespace
{
SessionStore sessions;
std::string uiUrl;

void saveSessions()
{
    writeDataFile("sessions", sessionsToJson(sessions));
}

HttpResponse api(const ApiResponse& r)
{
    return {r.status, "application/json", r.body, "", ""};
}

// API call that changes the sessions on success.
HttpResponse apiChange(const ApiResponse& r, int okStatus)
{
    if (r.status == okStatus)
    {
        saveSessions();
    }
    return api(r);
}

HttpResponse route(const HttpRequest& req)
{
    bool get = req.method == "GET";
    bool post = req.method == "POST";
    if (get && req.path == "/api/status")
    {
        return api(handleStatus(sessions, req.arg("kz"), nowUtc()));
    }
    if (get && req.path == "/api/sessions")
    {
        return api(handleSessions(sessions, nowUtc(), true));
    }
    if (post && req.path == "/api/entry")
    {
        return apiChange(handleEntry(sessions, req.body, nowUtc(), true), 201);
    }
    if (post && req.path == "/api/exit")
    {
        return apiChange(handleExit(sessions, req.body, nowUtc(), true), 200);
    }
    if (post && req.path == "/api/pay")
    {
        return apiChange(handlePay(sessions, req.body, nowUtc(), true), 200);
    }
    if (post && req.path == "/api/reset")
    {
        std::printf("sessions: test-data reset\n");
        return apiChange(handleReset(sessions), 200);
    }
    if (get && req.path == "/")
    {
        return htmlPage(renderKasseServerPage(uiUrl));
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
    uiUrl = envOr("SCHRANKEN_PUBLIC_URL", "http://localhost:8082");
    loadSessions();
    std::string host = listenHost();
    std::printf("Simulator Pico 1 «Kasse», %s, Daten in %s/\n", formatDateTime(nowUtc()).c_str(), dataDir().c_str());
    std::printf("  API auf Port %u (lauscht auf %s); Bedienung auf Pico 2: %s\n", KASSE_PORT, host.c_str(),
                uiUrl.c_str());
    std::printf("Beenden mit Ctrl+C\n");
    std::fflush(stdout);
    return runHttpServers(host, {{KASSE_PORT, route}}, []() { std::fflush(stdout); }) ? 0 : 1;
}
