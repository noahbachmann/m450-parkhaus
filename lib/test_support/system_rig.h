#pragma once

#include <map>
#include <memory>
#include <string>

#include "codec.h"
#include "format.h"
#include "local_barrier.h"
#include "schranken_app.h"
#include "test_fixtures.h"

// System test without hardware (test-concept.md, test environment TU-1): Pico 2 is the real SchrankenApp with
// LocalBarriers and files in memory, Pico 1 the real routeKasse; the tester uses the same routes and forms as in
// the browser. Time is simulated (seconds for parking, milliseconds for the barriers). «Pico 1 off» = no answer,
// like a timeout. A reboot rebuilds a Pico from its files.
namespace fixtures
{
    class SystemRig
    {
    public:
        LocalKasse kasse;
        LocalBarrier entryBarrier;
        LocalBarrier exitBarrier;
        MemoryPlatform platform;
        int64_t now = NOW;
        uint32_t nowMs = 1000;
        std::string cookie;

        SystemRig()
        {
            entryBarrier.update(nowMs);
            exitBarrier.update(nowMs);
            rebootSchranken();
        }

        SchrankenApp& app()
        {
            return *pico2;
        }

        const SessionStore& sessions() const
        {
            return kasse.store;
        }

        // A request as the browser sends it; redirects are not followed.
        WebResponse request(const char* method, const std::string& path,
                            const std::map<std::string, std::string>& args = {})
        {
            WebRequest req;
            req.method = method;
            req.path = path;
            req.args = args;
            req.cookie = cookie;
            kasse.now = now;
            return pico2->handle(req, now);
        }

        // GET, or POST and follow the redirect: the page the browser shows afterwards.
        std::string page(const std::string& path, const std::map<std::string, std::string>& args = {})
        {
            return request("GET", path, args).body;
        }

        std::string submit(const std::string& path, const std::map<std::string, std::string>& args = {})
        {
            WebResponse r = request("POST", path, args);
            if (r.status != 303)
            {
                return r.body;
            }
            std::map<std::string, std::string> query;
            size_t q = r.location.find('?');
            if (q != std::string::npos)
            {
                parseQuery(r.location.substr(q + 1), query);
            }
            return page(r.location.substr(0, q), query);
        }

        void login()
        {
            WebResponse r = request("POST", "/login", {{"user", "betrieb"}, {"password", "parkhaus"}});
            cookie = r.setCookie.substr(0, r.setCookie.find(';'));
        }

        // Camera simulation; returns the lane's terminal text.
        std::string entry(const std::string& kz, const std::string& entryTime = "")
        {
            request("POST", "/kamera/einfahrt", {{"kz", kz}, {"entry", entryTime}});
            return terminalText(GateLane::Entry);
        }

        // Entry backdated by seconds, typed into the form like a tester would (to the second).
        std::string entryAgo(const std::string& kz, int64_t seconds)
        {
            return entry(kz, formatDateTime(now - seconds));
        }

        std::string exit(const std::string& kz)
        {
            request("POST", "/kamera/ausfahrt", {{"kz", kz}});
            return terminalText(GateLane::Exit);
        }

        std::string driveThrough(GateLane lane)
        {
            request("POST", "/kamera/durchfahren", {{"lane", gateLaneName(lane)}});
            return terminalText(lane);
        }

        // «Fahrzeug bleibt stehen» / «Fahrzeug fährt weiter».
        void hold(GateLane lane, bool held)
        {
            request("POST", "/kamera/schleife", {{"lane", gateLaneName(lane)}, {"held", held ? "1" : "0"}});
        }

        std::string callButton(GateLane lane)
        {
            request("POST", "/kamera/ruftaste", {{"lane", gateLaneName(lane)}});
            return terminalText(lane);
        }

        std::string turnAround()
        {
            request("POST", "/kamera/abbrechen");
            return terminalText(GateLane::Exit);
        }

        std::string payAtExit(const char* method)
        {
            request("POST", "/kamera/bezahlen", {{"method", method}});
            return terminalText(GateLane::Exit);
        }

        // Automat page for the plate, and paying there (returns the page after paying).
        std::string automat(const std::string& kz)
        {
            return page("/automat", {{"kz", kz}});
        }

        std::string automatPay(const std::string& kz, const char* method, const char* amount = "")
        {
            return submit("/automat/pay", {{"kz", kz}, {"method", method}, {"amount", amount}});
        }

        // Operator: «Schranke öffnen» → «Bestätigen» (login first); returns the operator page afterwards.
        std::string remoteOpen(GateLane lane, const std::string& kz, const std::string& reason,
                               const std::string& name = "")
        {
            return submit("/betrieb/oeffnen", {{"lane", gateLaneName(lane)}, {"kz", kz}, {"reason", reason}, {"name", name}});
        }

        std::string terminalText(GateLane lane)
        {
            return pico2->view(lane).terminal;
        }

        BarrierState state(GateLane lane) const
        {
            return lane == GateLane::Entry ? entryBarrier.state() : exitBarrier.state();
        }

        // Clock in 1 s steps; the barriers run in every step.
        void wait(int64_t seconds)
        {
            for (int64_t i = 0; i < seconds; ++i)
            {
                now++;
                step(1000);
            }
        }

        // Clock in 100 ms steps (closing takes 2 s, so a loop can hit it mid-sweep).
        void waitMs(uint32_t ms)
        {
            for (uint32_t t = 0; t < ms; t += 100)
            {
                step(100);
            }
        }

        size_t eventCount(EventType type)
        {
            size_t n = 0;
            for (const Event& e : pico2->events().all())
            {
                n += e.type == type ? 1 : 0;
            }
            return n;
        }

        size_t sessionCount(const std::string& plate) const
        {
            size_t n = 0;
            for (const ParkingSession& s : kasse.store.all())
            {
                n += s.plate == plate ? 1 : 0;
            }
            return n;
        }

        // Pico 1 restarts: sessions back from the flash file.
        void rebootKasse()
        {
            std::string file = sessionsToJson(kasse.store);
            kasse.store = SessionStore();
            sessionsFromJson(file, kasse.store);
        }

        // Pico 2 restarts: registrations and log back from its files, logins and terminals are gone.
        void rebootSchranken()
        {
            pico2.reset(new SchrankenApp(kasse, entryBarrier, exitBarrier, platform, "parkhaus"));
            pico2->begin();
            cookie.clear();
        }

    private:
        void step(uint32_t ms)
        {
            nowMs += ms;
            pico2->onBarrierEvent(GateLane::Entry, entryBarrier.update(nowMs), now);
            pico2->onBarrierEvent(GateLane::Exit, exitBarrier.update(nowMs), now);
        }

        std::unique_ptr<SchrankenApp> pico2;
    };
}
