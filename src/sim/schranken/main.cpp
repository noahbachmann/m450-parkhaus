#include <cstdio>
#include <random>
#include <string>

#include "barrier.h"
#include "format.h"
#include "http_client.h"
#include "http_server.h"
#include "local_barrier.h"
#include "schranken_app.h"
#include "sim_common.h"

// Simulated Pico 2 "Schranken": lib/schranken_app with two LocalBarriers, real HTTP calls to the Kasse at
// KASSE_URL (default http://localhost:8081). No safety loops: «Durchfahren» and «Fahrzeug bleibt stehen» on the
// camera page stand in for them. Pico 1 off = stop the Kasse program or container.
// Login: user «betrieb», password OPERATOR_PASSWORD (default "parkhaus").

namespace
{
constexpr int KASSE_TIMEOUT_MS = 1000;

class HttpKasseLink : public KasseLink
{
public:
    explicit HttpKasseLink(std::string url) : baseUrl(std::move(url))
    {
    }

    HttpReply postJson(const char* path, const std::string& body) override
    {
        HttpReply r = httpPostJson(baseUrl, path, body, KASSE_TIMEOUT_MS);
        std::printf("[kasse] POST %s %s -> %d %s\n", path, body.c_str(), r.status,
                    r.status > 0 ? r.body.c_str() : "(keine Antwort)");
        return r;
    }

    HttpReply getJson(const std::string& pathAndQuery) override
    {
        return httpGetJson(baseUrl, pathAndQuery, KASSE_TIMEOUT_MS);
    }

private:
    std::string baseUrl;
};

class SimPlatform : public AppPlatform
{
public:
    bool readFile(const char* name, std::string& json) override
    {
        return readDataFile(name, json);
    }

    void writeFile(const char* name, const std::string& json) override
    {
        writeDataFile(name, json);
    }

    uint32_t random32() override
    {
        return randomSource();
    }

    void log(const std::string& line) override
    {
        std::printf("%s\n", line.c_str());
    }

private:
    std::random_device randomSource;
};

struct Lane
{
    GateLane id;
    LocalBarrier barrier;
};

Lane entryLane{GateLane::Entry, {}};
Lane exitLane{GateLane::Exit, {}};
SchrankenApp* app = nullptr;

void updateLane(Lane& lane, uint32_t ms)
{
    BarrierState before = lane.barrier.state();
    app->onBarrierEvent(lane.id, lane.barrier.update(ms), nowUtc());
    if (lane.barrier.state() != before)
    {
        std::printf("[barrier] %s: %s\n", gateLaneLabel(lane.id), barrierStateLabel(lane.barrier.state()));
    }
}

void tick()
{
    uint32_t ms = nowMs();
    updateLane(entryLane, ms);
    updateLane(exitLane, ms);
    std::fflush(stdout);
}
}

int main()
{
    std::string kasseUrl = envOr("KASSE_URL", "http://localhost:8081");
    HttpKasseLink kasse(kasseUrl);
    SimPlatform platform;
    SchrankenApp schranken(kasse, entryLane.barrier, exitLane.barrier, platform,
                           envOr("OPERATOR_PASSWORD", "parkhaus"));
    app = &schranken;
    makeDataDir();
    schranken.begin();
    tick();

    std::string host = listenHost();
    std::printf("Simulator Pico 2 «Schranken», %s, Daten in %s/, Kasse unter %s\n", formatDateTime(nowUtc()).c_str(),
                dataDir().c_str(), kasseUrl.c_str());
    std::printf("  http://localhost:%u/kamera   (lauscht auf %s)\n", SCHRANKEN_PORT, host.c_str());
    std::printf("  Anmelden: http://localhost:%u/login   (Benutzer «betrieb», Passwort aus OPERATOR_PASSWORD)\n",
                SCHRANKEN_PORT);
    std::printf("Beenden mit Ctrl+C\n");
    std::fflush(stdout);
    auto route = [&schranken](const WebRequest& req) { return schranken.handle(req, nowUtc()); };
    return runHttpServers(host, {{SCHRANKEN_PORT, route}}, tick) ? 0 : 1;
}
