#include <unity.h>

#include "kasse_api.h"
#include "operator_view.h"
#include "payment.h"
#include "session.h"

namespace
{
    constexpr int64_t NOW = 1760530000; // 15.10.2025 14:06:40 Zurich (CEST)
    constexpr int64_t MIN = 60;

    bool contains(const std::string& text, const std::string& part)
    {
        return text.find(part) != std::string::npos;
    }

    // Pico 1's real API handlers on a real store; online switch for "Pico 1 off".
    class LocalKasse : public KasseLink
    {
    public:
        SessionStore store;
        bool online = true;

        HttpReply postJson(const char* path, const std::string& body) override
        {
            if (!online)
            {
                return {};
            }
            ApiResponse r = std::string(path) == "/api/entry" ? handleEntry(store, body, NOW, true)
                                                               : handleExit(store, body, NOW, true);
            return {r.status, r.body};
        }

        HttpReply getJson(const std::string& pathAndQuery) override
        {
            if (!online)
            {
                return {};
            }
            ApiResponse r = handleStatus(store, pathAndQuery.substr(pathAndQuery.find('=') + 1), NOW);
            return {r.status, r.body};
        }
    };
}

void setUp()
{
}

void tearDown()
{
}

void test_lane_status_us08()
{
    LocalKasse kasse;
    kasse.store.openEntry("LU123456", NOW - 80 * MIN);
    pay(kasse.store, "LU123456", PayMethod::CardOk, NOW);
    kasse.store.openEntry("ZH98765", NOW - 150 * MIN);
    TEST_ASSERT_EQUAL_STRING("bezahlt", laneStatus(kasse, "LU123456").c_str());
    TEST_ASSERT_EQUAL_STRING("unbezahlt (CHF 6.00)", laneStatus(kasse, "ZH98765").c_str());
    TEST_ASSERT_EQUAL_STRING("kein Parkvorgang", laneStatus(kasse, "BE4711").c_str());
    TEST_ASSERT_EQUAL_STRING("–", laneStatus(kasse, "").c_str());
    kasse.online = false;
    TEST_ASSERT_EQUAL_STRING("Kasse nicht erreichbar", laneStatus(kasse, "LU123456").c_str());
}

void test_remote_open_entry_opens_session_tf16()
{
    LocalKasse kasse;
    EventLog log;
    RemoteOpenResult r = remoteOpen(GateLane::Entry, "BE4711", "Kennzeichen nicht erkannt", NOW, kasse, log);
    TEST_ASSERT_TRUE(r.open);
    TEST_ASSERT_TRUE(r.logged);
    TEST_ASSERT_EQUAL_STRING("Schranke Einfahrt geöffnet – Parkvorgang eröffnet", r.notice.text.c_str());
    TEST_ASSERT_NOT_NULL(kasse.store.findActive("BE4711"));
    TEST_ASSERT_EQUAL_UINT(1, log.all().size());
    const Event& e = log.all()[0];
    TEST_ASSERT_EQUAL(EventType::RemoteOpening, e.type);
    TEST_ASSERT_EQUAL(GateLane::Entry, e.lane);
    TEST_ASSERT_EQUAL_INT64(NOW, e.time);
    TEST_ASSERT_EQUAL_STRING("BE4711", e.plate.c_str());
    TEST_ASSERT_EQUAL_STRING("betrieb", e.user.c_str());
    TEST_ASSERT_EQUAL_STRING("Kennzeichen nicht erkannt", e.reason.c_str());
}

void test_remote_open_without_reason_tf17()
{
    LocalKasse kasse;
    EventLog log;
    for (const char* reason : {"", "weil ich will"})
    {
        RemoteOpenResult r = remoteOpen(GateLane::Entry, "BE4711", reason, NOW, kasse, log);
        TEST_ASSERT_FALSE(r.open);
        TEST_ASSERT_FALSE(r.logged);
        TEST_ASSERT_EQUAL_STRING("Bitte Grund auswählen", r.notice.text.c_str());
    }
    TEST_ASSERT_EQUAL_UINT(0, log.all().size());
    TEST_ASSERT_EQUAL_UINT(0, kasse.store.all().size());
}

void test_remote_open_exit_unpaid_tf31()
{
    LocalKasse kasse;
    EventLog log;
    kasse.store.openEntry("ZH98765", NOW - 150 * MIN);
    RemoteOpenResult r = remoteOpen(GateLane::Exit, "ZH98765", "Kulanz", NOW, kasse, log);
    TEST_ASSERT_TRUE(r.open);
    TEST_ASSERT_EQUAL_STRING("Schranke Ausfahrt geöffnet – ohne Zahlung", r.notice.text.c_str());
    const ParkingSession* s = kasse.store.findLatest("ZH98765");
    TEST_ASSERT_EQUAL(SessionStatus::ClosedUnpaid, s->status);
    TEST_ASSERT_EQUAL_STRING("ohne Zahlung ausgefahren", sessionStatusLabel(s->status));
    const Event& e = log.all()[0];
    TEST_ASSERT_EQUAL(GateLane::Exit, e.lane);
    TEST_ASSERT_EQUAL_STRING("Kulanz", e.reason.c_str());
    TEST_ASSERT_EQUAL_STRING("ohne Zahlung", e.note.c_str());
}

void test_remote_open_exit_paid_and_without_session()
{
    LocalKasse kasse;
    EventLog log;
    kasse.store.openEntry("LU123456", NOW - 80 * MIN);
    pay(kasse.store, "LU123456", PayMethod::Twint, NOW);
    TEST_ASSERT_EQUAL_STRING("Schranke Ausfahrt geöffnet – bezahlt",
                             remoteOpen(GateLane::Exit, "LU123456", "Technische Störung", NOW, kasse, log)
                                 .notice.text.c_str());
    TEST_ASSERT_EQUAL(SessionStatus::Closed, kasse.store.findLatest("LU123456")->status);
    TEST_ASSERT_EQUAL_STRING("Schranke Ausfahrt geöffnet – kein Parkvorgang",
                             remoteOpen(GateLane::Exit, "SZ555", "Kulanz", NOW, kasse, log).notice.text.c_str());
    TEST_ASSERT_EQUAL_UINT(2, log.all().size());
}

void test_remote_open_with_kasse_off_stays_closed()
{
    LocalKasse kasse;
    kasse.online = false;
    EventLog log;
    for (GateLane lane : {GateLane::Entry, GateLane::Exit})
    {
        RemoteOpenResult r = remoteOpen(lane, "BE4711", "Kennzeichen nicht erkannt", NOW, kasse, log);
        TEST_ASSERT_FALSE(r.open);
        TEST_ASSERT_TRUE(r.logged);
        TEST_ASSERT_EQUAL_STRING("Kasse nicht erreichbar – Schranke bleibt zu", r.notice.text.c_str());
    }
    TEST_ASSERT_EQUAL_UINT(2, log.all().size());
    const Event& e = log.all()[0];
    TEST_ASSERT_EQUAL(EventType::KasseUnreachable, e.type);
    TEST_ASSERT_EQUAL_STRING("betrieb", e.user.c_str());
    TEST_ASSERT_EQUAL_STRING("Kennzeichen nicht erkannt", e.reason.c_str());
    TEST_ASSERT_EQUAL_STRING("Fernöffnung abgebrochen, Schranke bleibt zu", e.note.c_str());
}

void test_remote_open_needs_valid_plate()
{
    LocalKasse kasse;
    EventLog log;
    for (const char* kz : {"", "LU-12#45"})
    {
        RemoteOpenResult r = remoteOpen(GateLane::Entry, kz, "Kennzeichen nicht erkannt", NOW, kasse, log);
        TEST_ASSERT_FALSE(r.open);
        TEST_ASSERT_FALSE(r.logged);
        TEST_ASSERT_EQUAL_STRING("Bitte gültiges Kennzeichen eingeben", r.notice.text.c_str());
    }
    TEST_ASSERT_EQUAL_UINT(0, log.all().size());
}

void test_remote_open_unexpected_reply_stays_closed()
{
    class BrokenKasse : public KasseLink
    {
    public:
        HttpReply postJson(const char*, const std::string&) override
        {
            return {503, "{\"error\":\"time_not_synced\"}"};
        }
        HttpReply getJson(const std::string&) override
        {
            return {};
        }
    } kasse;
    EventLog log;
    RemoteOpenResult r = remoteOpen(GateLane::Exit, "ZH98765", "Kulanz", NOW, kasse, log);
    TEST_ASSERT_FALSE(r.open);
    TEST_ASSERT_FALSE(r.logged);
    TEST_ASSERT_EQUAL_STRING("Unerwartete Antwort der Kasse (503) – Schranke bleibt zu", r.notice.text.c_str());
}

void test_log_helpers_tf14_tf15_tf30()
{
    EventLog log;
    TEST_ASSERT_FALSE(logBarrierEvent(log, GateLane::Entry, BarrierEvent::None, NOW));
    TEST_ASSERT_TRUE(logBarrierEvent(log, GateLane::Exit, BarrierEvent::SafetyStop, NOW));
    TEST_ASSERT_TRUE(logBarrierEvent(log, GateLane::Entry, BarrierEvent::LoopFault, NOW));
    TEST_ASSERT_EQUAL_STRING("Sicherungsschleife Einfahrt länger als 5 min belegt", log.all()[1].note.c_str());

    GateResult ok;
    ok.open = true;
    TEST_ASSERT_FALSE(logGateResult(log, GateLane::Exit, ok, NOW));
    GateResult down;
    down.unreachable = true;
    down.plate = "ZH98765";
    down.message = "Kasse nicht erreichbar – bitte Ruftaste drücken";
    TEST_ASSERT_TRUE(logGateResult(log, GateLane::Exit, down, NOW));
    TEST_ASSERT_EQUAL(EventType::KasseUnreachable, log.all()[2].type);
    TEST_ASSERT_EQUAL_STRING("ZH98765", log.all()[2].plate.c_str());
}

void test_operator_page_filter_tf14()
{
    EventLog log;
    for (int i = 0; i < 6; ++i)
    {
        logBarrierEvent(log, i % 2 == 0 ? GateLane::Entry : GateLane::Exit, BarrierEvent::SafetyStop, NOW + i);
    }
    LocalKasse kasse;
    remoteOpen(GateLane::Entry, "BE4711", "Kennzeichen nicht erkannt", NOW + 10, kasse, log);

    OperatorLane entry{GateLane::Entry, "offen", "BE4711", "kein Parkvorgang", ""};
    OperatorLane exit{GateLane::Exit, "geschlossen", "", "", "Sicherungsschleife Ausfahrt länger als 5 min belegt"};
    std::string all = renderOperatorPage(entry, exit, log, "", Notice{});
    TEST_ASSERT_TRUE(contains(all, "7 Einträge"));
    TEST_ASSERT_TRUE(contains(all, "<td>BE4711</td>"));
    TEST_ASSERT_TRUE(contains(all, "<td>kein Parkvorgang</td>"));
    TEST_ASSERT_TRUE(contains(all, "value=\"BE4711\""));
    TEST_ASSERT_TRUE(contains(all, "<option>Kennzeichen nicht erkannt</option>"));
    TEST_ASSERT_TRUE(contains(all, "Bestätigen"));
    TEST_ASSERT_TRUE(contains(all, "notice err\">Sicherungsschleife Ausfahrt länger als 5 min belegt"));
    TEST_ASSERT_TRUE(contains(all, "Fernöffnung"));

    std::string stops = renderOperatorPage(entry, exit, log, "safety_stop", Notice{});
    TEST_ASSERT_TRUE(contains(stops, "6 Einträge"));
    TEST_ASSERT_FALSE(contains(stops, "<td>Fernöffnung</td>"));
    TEST_ASSERT_TRUE(contains(stops, "<td>15.10.2025 14:06:45</td><td>Sicherheitsabschaltung</td><td>Ausfahrt</td>"));
    TEST_ASSERT_TRUE(contains(stops, "value=\"safety_stop\" selected"));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_lane_status_us08);
    RUN_TEST(test_remote_open_entry_opens_session_tf16);
    RUN_TEST(test_remote_open_without_reason_tf17);
    RUN_TEST(test_remote_open_exit_unpaid_tf31);
    RUN_TEST(test_remote_open_exit_paid_and_without_session);
    RUN_TEST(test_remote_open_with_kasse_off_stays_closed);
    RUN_TEST(test_remote_open_needs_valid_plate);
    RUN_TEST(test_remote_open_unexpected_reply_stays_closed);
    RUN_TEST(test_log_helpers_tf14_tf15_tf30);
    RUN_TEST(test_operator_page_filter_tf14);
    return UNITY_END();
}
