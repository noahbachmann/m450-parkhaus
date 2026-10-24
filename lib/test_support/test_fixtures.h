#pragma once

#include <map>
#include <string>

#include "gate.h"
#include "kasse_api.h"
#include "schranken_app.h"
#include "session.h"
#include "web.h"

// Shared helpers of the native test suites (header only, not used by the firmware).

namespace fixtures
{
    constexpr int64_t NOW = 1760530000; // 15.10.2025 14:06:40 Zurich (CEST)
    constexpr int64_t MIN = 60;
    constexpr int64_t HOUR = 3600;

    // Test plates, test-concept.md 11.2.1 (as typed by the tester).
    constexpr const char* KZ_01 = "LU 123456"; // registered, Test Kunde A
    constexpr const char* KZ_02 = "ZH 98765";  // registered, Test Kunde B
    constexpr const char* KZ_03 = "BE 4711";   // not registered
    constexpr const char* KZ_04 = "lu 123 456"; // spelling variant of KZ-01
    constexpr const char* KZ_05 = "";           // invalid: empty
    constexpr const char* KZ_06 = "LU-12#45";   // invalid: special characters
    constexpr const char* KZ_07 = "LU 1234567"; // invalid: 7 digits
    constexpr const char* KZ_08 = "AG 1";       // registered, shortest
    constexpr const char* KZ_09 = "L 123";      // invalid: 1 letter
    constexpr const char* KZ_10 = "SZ 555";     // registered, no session
    constexpr const char* KZ_11 = "OW 2026";    // not registered at first (TF-32)
    constexpr const char* KZ_12 = "NW 654321";  // not registered (TF-01)
    constexpr const char* KZ_13 = "UR 1";       // not registered, shortest (TF-02)

    inline bool contains(const std::string& text, const std::string& part)
    {
        return text.find(part) != std::string::npos;
    }

    // Pico 1 behind its real router (routeKasse), as Pico 2 reaches it over HTTP.
    class LocalKasse : public KasseLink
    {
    public:
        SessionStore store;
        int64_t now = NOW;
        bool online = true; // false: no answer, like a timeout
        bool synced = true;
        int calls = 0;

        HttpReply postJson(const char* path, const std::string& body) override
        {
            WebRequest req;
            req.method = "POST";
            req.path = path;
            req.body = body;
            return send(req);
        }

        HttpReply getJson(const std::string& pathAndQuery) override
        {
            WebRequest req;
            req.method = "GET";
            size_t q = pathAndQuery.find('?');
            req.path = pathAndQuery.substr(0, q);
            if (q != std::string::npos)
            {
                parseQuery(pathAndQuery.substr(q + 1), req.args);
            }
            return send(req);
        }

    private:
        HttpReply send(const WebRequest& req)
        {
            calls++;
            if (!online)
            {
                return {};
            }
            WebResponse r = routeKasse(req, store, now, synced, "http://pico2", true).response;
            return {r.status, r.body};
        }
    };

    // Always the same answer.
    class FixedReply : public KasseLink
    {
    public:
        HttpReply reply;

        HttpReply postJson(const char*, const std::string&) override
        {
            return reply;
        }

        HttpReply getJson(const std::string&) override
        {
            return reply;
        }
    };

    // Files in a map; counts the writes per file.
    class MemoryPlatform : public AppPlatform
    {
    public:
        std::map<std::string, std::string> files;
        std::map<std::string, int> writes;
        uint32_t nextRandom = 0x1000;

        bool readFile(const char* name, std::string& json) override
        {
            auto it = files.find(name);
            if (it == files.end())
            {
                return false;
            }
            json = it->second;
            return true;
        }

        void writeFile(const char* name, const std::string& json) override
        {
            files[name] = json;
            writes[name]++;
        }

        uint32_t random32() override
        {
            return nextRandom++;
        }

        void log(const std::string&) override
        {
        }
    };
}
