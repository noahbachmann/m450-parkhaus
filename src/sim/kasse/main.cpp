#include <cstdio>
#include <string>

#include "codec.h"
#include "format.h"
#include "http_server.h"
#include "kasse_api.h"
#include "sim_common.h"
#include "testdata.h"

// Simulated Pico 1 "Kasse": the routes of lib/kasse_api (routeKasse), sessions in SIM_DATA/sessions.json.
// GET / links to the pages on Pico 2 (SCHRANKEN_PUBLIC_URL).

namespace
{
SessionStore sessions;
std::string uiUrl;

void saveSessions()
{
    writeDataFile("sessions", sessionsToJson(sessions));
}

WebResponse route(const WebRequest& req)
{
    // The PC clock is always set, so the simulated Kasse is always synced.
    KasseRouteResult r = routeKasse(req, sessions, nowUtc(), true, uiUrl);
    if (r.changed)
    {
        saveSessions();
    }
    return r.response;
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
