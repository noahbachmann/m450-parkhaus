#include <unity.h>

#include <map>
#include <string>
#include <vector>

#include "kasse_api.h"
#include "schranken_app.h"

namespace
{
    constexpr int64_t NOW = 1760530000;
    constexpr int64_t MIN = 60;

    bool contains(const std::string& text, const std::string& part)
    {
        return text.find(part) != std::string::npos;
    }

    class FakeBarrier : public BarrierPort
    {
    public:
        BarrierState current = BarrierState::Closed;
        bool fault = false;
        bool held = false;
        bool passResult = false;
        int opens = 0;
        int passes = 0;

        BarrierState state() const override
        {
            return current;
        }
        bool loopFault() const override
        {
            return fault;
        }
        bool loopHeld() const override
        {
            return held;
        }
        void requestOpen() override
        {
            opens++;
            current = BarrierState::Open;
        }
        bool requestPass() override
        {
            passes++;
            return passResult;
        }
        void setLoopHeld(bool h) override
        {
            held = h;
        }
    };

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

    // Pico 1 through its real router.
    class LocalKasse : public KasseLink
    {
    public:
        SessionStore store;
        int64_t now = NOW;
        bool online = true;

        HttpReply postJson(const char* path, const std::string& body) override
        {
            return call("POST", path, body);
        }
        HttpReply getJson(const std::string& pathAndQuery) override
        {
            size_t q = pathAndQuery.find('?');
            WebRequest req;
            req.method = "GET";
            req.path = pathAndQuery.substr(0, q);
            if (q != std::string::npos)
            {
                parseQuery(pathAndQuery.substr(q + 1), req.args);
            }
            return send(req);
        }

    private:
        HttpReply call(const char* method, const char* path, const std::string& body)
        {
            WebRequest req;
            req.method = method;
            req.path = path;
            req.body = body;
            return send(req);
        }
        HttpReply send(const WebRequest& req)
        {
            if (!online)
            {
                return {};
            }
            WebResponse r = routeKasse(req, store, now, true, "http://ui").response;
            return {r.status, r.body};
        }
    };

    struct Rig
    {
        LocalKasse kasse;
        FakeBarrier entry;
        FakeBarrier exit;
        MemoryPlatform platform;
        SchrankenApp app{kasse, entry, exit, platform, "geheim"};
        std::string cookie;

        Rig()
        {
            app.begin();
            platform.writes.clear();
        }

        WebResponse get(const std::string& path, std::map<std::string, std::string> args = {})
        {
            return send("GET", path, std::move(args));
        }
        WebResponse post(const std::string& path, std::map<std::string, std::string> args = {})
        {
            return send("POST", path, std::move(args));
        }
        void login()
        {
            WebResponse r = post("/login", {{"user", "betrieb"}, {"password", "geheim"}});
            cookie = r.setCookie.substr(0, r.setCookie.find(';'));
        }
        // Follows the redirect of a POST and returns the page it leads to.
        std::string follow(const WebResponse& r)
        {
            std::string location = r.location;
            size_t q = location.find('?');
            WebRequest req;
            req.method = "GET";
            req.path = location.substr(0, q);
            if (q != std::string::npos)
            {
                parseQuery(location.substr(q + 1), req.args);
            }
            req.cookie = cookie;
            return app.handle(req, NOW).body;
        }

    private:
        WebResponse send(const char* method, const std::string& path, std::map<std::string, std::string> args)
        {
            WebRequest req;
            req.method = method;
            req.path = path;
            req.args = std::move(args);
            req.cookie = cookie;
            return app.handle(req, NOW);
        }
    };

    const char* const ADMIN_ROUTES[][2] = {
        {"GET", "/kasse"},          {"POST", "/kasse/pay"},       {"GET", "/registrierung"},
        {"POST", "/registrierung/add"}, {"POST", "/registrierung/delete"}, {"GET", "/betrieb"},
        {"POST", "/betrieb/oeffnen"}, {"POST", "/reset"},
    };
}

void setUp()
{
}

void tearDown()
{
}

void test_public_routes()
{
    Rig rig;
    TEST_ASSERT_EQUAL_INT(303, rig.get("/").status);
    TEST_ASSERT_EQUAL_STRING("/kamera", rig.get("/").location.c_str());
    TEST_ASSERT_EQUAL_INT(200, rig.get("/kamera").status);
    TEST_ASSERT_EQUAL_INT(200, rig.get("/tarif").status);
    TEST_ASSERT_EQUAL_INT(200, rig.get("/login").status);
    for (const char* path : {"/kamera/einfahrt", "/kamera/ausfahrt", "/kamera/ruftaste", "/kamera/durchfahren",
                             "/kamera/schleife", "/kamera/abbrechen", "/kamera/bezahlen", "/logout"})
    {
        WebResponse r = rig.post(path);
        TEST_ASSERT_EQUAL_INT_MESSAGE(303, r.status, path);
    }
}

void test_unknown_route_or_method_is_404_for_everyone()
{
    Rig rig;
    TEST_ASSERT_EQUAL_INT(404, rig.get("/nope").status);
    TEST_ASSERT_EQUAL_INT(404, rig.get("/kasse/pay").status);
    TEST_ASSERT_EQUAL_INT(404, rig.post("/kamera").status);
    rig.login();
    TEST_ASSERT_EQUAL_INT(404, rig.get("/kasse/pay").status);
    TEST_ASSERT_EQUAL_INT(404, rig.get("/reset").status);
}

void test_admin_routes_need_login()
{
    Rig rig;
    for (const auto& route : ADMIN_ROUTES)
    {
        TEST_ASSERT_TRUE_MESSAGE(SchrankenApp::isAdminRoute(route[0], route[1]), route[1]);
        WebResponse r = std::string(route[0]) == "GET" ? rig.get(route[1]) : rig.post(route[1]);
        TEST_ASSERT_EQUAL_INT_MESSAGE(303, r.status, route[1]);
        TEST_ASSERT_TRUE_MESSAGE(contains(r.location, "/login?next="), route[1]);
    }
    TEST_ASSERT_EQUAL_STRING("/login?next=/kasse", rig.get("/kasse").location.c_str());
    TEST_ASSERT_EQUAL_INT(0, rig.platform.writes["registrations"] + rig.platform.writes["events"]);
    TEST_ASSERT_FALSE(SchrankenApp::isAdminRoute("GET", "/kamera"));

    rig.login();
    TEST_ASSERT_EQUAL_INT(200, rig.get("/kasse").status);
    TEST_ASSERT_EQUAL_INT(200, rig.get("/registrierung").status);
    TEST_ASSERT_EQUAL_INT(200, rig.get("/betrieb").status);
}

void test_login_target_only_admin_pages()
{
    TEST_ASSERT_EQUAL_STRING("/kasse", SchrankenApp::loginTarget("/kasse").c_str());
    TEST_ASSERT_EQUAL_STRING("/registrierung", SchrankenApp::loginTarget("/registrierung").c_str());
    TEST_ASSERT_EQUAL_STRING("/betrieb", SchrankenApp::loginTarget("").c_str());
    TEST_ASSERT_EQUAL_STRING("/betrieb", SchrankenApp::loginTarget("http://evil.example").c_str());
    TEST_ASSERT_EQUAL_STRING("/betrieb", SchrankenApp::loginTarget("//evil.example").c_str());
    TEST_ASSERT_EQUAL_STRING("/betrieb", SchrankenApp::loginTarget("/kasse/pay").c_str());
    TEST_ASSERT_EQUAL_STRING("/betrieb", SchrankenApp::loginTarget("/kamera").c_str());
}

void test_login_logout_and_wrong_password()
{
    Rig rig;
    WebResponse wrong = rig.post("/login", {{"user", "betrieb"}, {"password", "falsch"}, {"next", "/kasse"}});
    TEST_ASSERT_EQUAL_STRING("/login?next=/kasse&m=1", wrong.location.c_str());
    TEST_ASSERT_TRUE(wrong.setCookie.empty());
    TEST_ASSERT_TRUE(contains(rig.follow(wrong), "Benutzer oder Passwort falsch"));
    TEST_ASSERT_FALSE(contains(rig.get("/login", {{"next", "/kasse"}}).body, "falsch")); // only with ?m=1

    WebResponse ok = rig.post("/login", {{"user", "betrieb"}, {"password", "geheim"}, {"next", "/registrierung"}});
    TEST_ASSERT_EQUAL_STRING("/registrierung", ok.location.c_str());
    TEST_ASSERT_TRUE(contains(ok.setCookie, "parkhaus_session=00001000000010010000100200001003"));
    rig.cookie = ok.setCookie.substr(0, ok.setCookie.find(';'));
    TEST_ASSERT_EQUAL_STRING("/registrierung", rig.get("/login", {{"next", "/registrierung"}}).location.c_str());

    WebResponse out = rig.post("/logout");
    TEST_ASSERT_TRUE(contains(out.setCookie, "Max-Age=0"));
    TEST_ASSERT_EQUAL_INT(303, rig.get("/betrieb").status); // token no longer valid
}

void test_notice_is_shown_once_per_post()
{
    Rig rig;
    rig.login();
    WebResponse added = rig.post("/registrierung/add", {{"name", "Test Kunde C"}, {"kz", "NW 654321"}});
    TEST_ASSERT_EQUAL_STRING("/registrierung?m=1", added.location.c_str());
    TEST_ASSERT_TRUE(contains(rig.follow(added), "Kennzeichen registriert: NW654321"));
    TEST_ASSERT_FALSE(contains(rig.get("/registrierung").body, "Kennzeichen registriert"));
    WebResponse dup = rig.post("/registrierung/add", {{"name", "Test Kunde B"}, {"kz", "nw654321"}});
    TEST_ASSERT_EQUAL_STRING("/registrierung?m=2", dup.location.c_str());
    TEST_ASSERT_FALSE(contains(rig.get("/registrierung", {{"m", "1"}}).body, "registriert:"));
    TEST_ASSERT_TRUE(contains(rig.follow(dup), "Kennzeichen bereits registriert"));
}

void test_kasse_pay_redirect_keeps_valid_plate()
{
    Rig rig;
    rig.login();
    rig.kasse.store.openEntry("LU123456", NOW - 80 * MIN);
    WebResponse paid = rig.post("/kasse/pay", {{"kz", "lu 123456"}, {"method", "card_ok"}});
    TEST_ASSERT_EQUAL_STRING("/kasse?kz=LU123456&m=1", paid.location.c_str());
    TEST_ASSERT_TRUE(contains(rig.follow(paid), "Bezahlt – CHF 4.00, Karte"));
    WebResponse bad = rig.post("/kasse/pay", {{"kz", "L1"}, {"method", "card_ok"}});
    TEST_ASSERT_EQUAL_STRING("/kasse?m=2", bad.location.c_str());
}

void test_entry_opens_and_terminal_shows_plate()
{
    Rig rig;
    rig.post("/kamera/einfahrt", {{"kz", "lu 123 456"}});
    TEST_ASSERT_EQUAL_INT(1, rig.entry.opens);
    TEST_ASSERT_EQUAL_STRING("LU123456", rig.app.terminal(GateLane::Entry).lastPlate.c_str());
    TEST_ASSERT_TRUE(contains(rig.get("/kamera").body, "LU123456 – Willkommen, Einfahrt frei"));

    rig.post("/kamera/einfahrt", {{"kz", "BE 4711"}});
    TEST_ASSERT_EQUAL_INT(1, rig.entry.opens);
    TEST_ASSERT_TRUE(contains(rig.get("/kamera").body, "BE4711 – nicht registriert, bitte Ruftaste drücken"));
}

void test_drive_through_uses_the_barrier_answer()
{
    Rig rig;
    rig.entry.current = BarrierState::Open; // the page thinks it is open ...
    rig.entry.passResult = false;           // ... but the barrier refuses (e.g. closed meanwhile)
    rig.post("/kamera/durchfahren", {{"lane", "entry"}});
    TEST_ASSERT_EQUAL_INT(1, rig.entry.passes);
    TEST_ASSERT_TRUE(contains(rig.get("/kamera").body, "Keine offene Schranke – Fahrzeug kann nicht durchfahren"));

    rig.post("/kamera/einfahrt", {{"kz", "LU123456"}});
    rig.entry.passResult = true;
    rig.post("/kamera/durchfahren", {{"lane", "entry"}});
    TEST_ASSERT_TRUE(contains(rig.get("/kamera").body, "LU123456 durchgefahren – Schranke schliesst"));
    rig.entry.current = BarrierState::Closed;
    TEST_ASSERT_TRUE(contains(rig.get("/kamera").body, "LU123456 durchgefahren – Schranke geschlossen"));
    TEST_ASSERT_EQUAL_INT(0, rig.exit.passes);
}

void test_loop_hold_only_while_barrier_up()
{
    Rig rig;
    rig.post("/kamera/schleife", {{"lane", "exit"}, {"held", "1"}});
    TEST_ASSERT_FALSE(rig.exit.held); // no car on the loop under a closed barrier

    rig.exit.current = BarrierState::Closing;
    rig.post("/kamera/schleife", {{"lane", "exit"}, {"held", "1"}});
    TEST_ASSERT_TRUE(rig.exit.held);
    std::string page = rig.get("/kamera").body;
    TEST_ASSERT_TRUE(contains(page, "Fahrzeug fährt weiter"));
    TEST_ASSERT_FALSE(contains(page, "<strong>Durchfahren</strong>"));

    rig.post("/kamera/schleife", {{"lane", "exit"}, {"held", "0"}});
    TEST_ASSERT_FALSE(rig.exit.held);
}

void test_saves_only_what_changed()
{
    Rig rig;
    rig.login();
    rig.post("/registrierung/add", {{"name", "Test Kunde C"}, {"kz", "NW 654321"}});
    TEST_ASSERT_EQUAL_INT(1, rig.platform.writes["registrations"]);
    TEST_ASSERT_EQUAL_INT(0, rig.platform.writes["events"]);
    rig.post("/registrierung/add", {{"name", "Test Kunde C"}, {"kz", "NW 654321"}}); // duplicate
    TEST_ASSERT_EQUAL_INT(1, rig.platform.writes["registrations"]);

    rig.post("/kamera/ruftaste", {{"lane", "entry"}});
    TEST_ASSERT_EQUAL_INT(1, rig.platform.writes["events"]);

    rig.kasse.online = false;
    rig.post("/kamera/einfahrt", {{"kz", "LU123456"}});
    TEST_ASSERT_EQUAL_INT(2, rig.platform.writes["events"]); // «Kasse nicht erreichbar» logged
    rig.kasse.online = true;

    rig.post("/betrieb/oeffnen",
             {{"lane", "entry"}, {"kz", "OW 2026"}, {"reason", "Neuer Kunde registrieren"}, {"name", "Test Kunde F"}});
    TEST_ASSERT_EQUAL_INT(2, rig.platform.writes["registrations"]);
    TEST_ASSERT_EQUAL_INT(3, rig.platform.writes["events"]);
    TEST_ASSERT_EQUAL_INT(1, rig.entry.opens);

    rig.app.onBarrierEvent(GateLane::Exit, BarrierEvent::None, NOW);
    TEST_ASSERT_EQUAL_INT(3, rig.platform.writes["events"]);
    rig.app.onBarrierEvent(GateLane::Exit, BarrierEvent::SafetyStop, NOW);
    TEST_ASSERT_EQUAL_INT(4, rig.platform.writes["events"]);
}

void test_payment_alert_saved_with_full_log()
{
    Rig rig;
    for (size_t i = 0; i < MAX_EVENTS; ++i)
    {
        rig.app.onBarrierEvent(GateLane::Entry, BarrierEvent::SafetyStop, NOW);
    }
    TEST_ASSERT_EQUAL_UINT(MAX_EVENTS, rig.app.events().all().size());
    rig.kasse.store.openEntry("ZH98765", NOW - 150 * MIN);
    rig.post("/kamera/ausfahrt", {{"kz", "ZH98765"}});
    int before = rig.platform.writes["events"];
    for (int i = 0; i < MAX_DECLINES; ++i)
    {
        rig.post("/kamera/bezahlen", {{"method", "card_declined"}});
    }
    TEST_ASSERT_EQUAL_INT(before + 1, rig.platform.writes["events"]);
    TEST_ASSERT_TRUE(EventType::PaymentAlert == rig.app.events().all().back().type);
    TEST_ASSERT_TRUE(contains(rig.platform.files["events"], "payment_alert"));
}

void test_reset_restores_defaults_and_resets_kasse()
{
    Rig rig;
    rig.login();
    rig.post("/registrierung/delete", {{"kz", "LU123456"}});
    rig.post("/kamera/ruftaste", {{"lane", "exit"}});
    rig.kasse.store.openEntry("ZH98765", NOW - 10 * MIN);
    WebResponse r = rig.post("/reset");
    TEST_ASSERT_TRUE(contains(rig.follow(r), "Testdaten zurückgesetzt"));
    TEST_ASSERT_TRUE(rig.app.registrations().contains("LU123456"));
    TEST_ASSERT_EQUAL_UINT(0, rig.app.events().all().size());
    TEST_ASSERT_EQUAL_UINT(0, rig.kasse.store.all().size());
    TEST_ASSERT_TRUE(contains(rig.platform.files["events"], "\"events\":[]"));

    rig.kasse.online = false;
    rig.post("/registrierung/delete", {{"kz", "LU123456"}});
    TEST_ASSERT_TRUE(contains(rig.follow(rig.post("/reset")), "Kasse nicht erreichbar, Parkvorgänge nicht zurückgesetzt"));
    TEST_ASSERT_TRUE(rig.app.registrations().contains("LU123456"));
}

void test_begin_loads_files_or_writes_defaults()
{
    LocalKasse kasse;
    FakeBarrier entry;
    FakeBarrier exit;
    MemoryPlatform platform;
    platform.files["registrations"] = "{\"registrations\":[{\"name\":\"X\",\"kz\":\"UR1\"}]}";
    platform.files["events"] = "kaputt";
    SchrankenApp app(kasse, entry, exit, platform, "geheim");
    app.begin();
    TEST_ASSERT_EQUAL_UINT(1, app.registrations().all().size());
    TEST_ASSERT_EQUAL_INT(0, platform.writes["registrations"]);
    TEST_ASSERT_EQUAL_INT(1, platform.writes["events"]);
    TEST_ASSERT_EQUAL_STRING("{\"events\":[]}", platform.files["events"].c_str());
}

void test_operator_page_shows_lane_fault_and_last_plate()
{
    Rig rig;
    rig.login();
    rig.post("/kamera/einfahrt", {{"kz", "BE4711"}});
    rig.entry.fault = true;
    std::string page = rig.get("/betrieb").body;
    TEST_ASSERT_TRUE(contains(page, "BE4711"));
    TEST_ASSERT_TRUE(contains(page, "Sicherungsschleife Einfahrt länger als 5 min belegt"));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_public_routes);
    RUN_TEST(test_unknown_route_or_method_is_404_for_everyone);
    RUN_TEST(test_admin_routes_need_login);
    RUN_TEST(test_login_target_only_admin_pages);
    RUN_TEST(test_login_logout_and_wrong_password);
    RUN_TEST(test_notice_is_shown_once_per_post);
    RUN_TEST(test_kasse_pay_redirect_keeps_valid_plate);
    RUN_TEST(test_entry_opens_and_terminal_shows_plate);
    RUN_TEST(test_drive_through_uses_the_barrier_answer);
    RUN_TEST(test_loop_hold_only_while_barrier_up);
    RUN_TEST(test_saves_only_what_changed);
    RUN_TEST(test_payment_alert_saved_with_full_log);
    RUN_TEST(test_reset_restores_defaults_and_resets_kasse);
    RUN_TEST(test_begin_loads_files_or_writes_defaults);
    RUN_TEST(test_operator_page_shows_lane_fault_and_last_plate);
    return UNITY_END();
}
