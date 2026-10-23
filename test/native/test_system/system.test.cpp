#include <unity.h>

#include <string>

#include "barrier.h"
#include "codec.h"
#include "format.h"
#include "gate.h"
#include "kasse_api.h"
#include "kasse_pages.h"
#include "kasse_view.h"
#include "operator_view.h"
#include "registration.h"
#include "schranken_view.h"
#include "session.h"
#include "testdata.h"

// Phase-1 system test (test-concept.md, P1 cases) without hardware: Pico 2 logic (registrations, gate,
// barriers) talks to Pico 1 logic (kasse_api handlers, pay form, pages) through an in-process link.
// A "reboot" is a round-trip through the flash file format.

namespace
{
    constexpr int64_t START = 1760530000; // 15.10.2025 14:06:40 Zurich (CEST)
    constexpr int64_t MIN = 60;
    constexpr int64_t HOUR = 3600;

    bool contains(const std::string& text, const std::string& part)
    {
        return text.find(part) != std::string::npos;
    }

    // "/api/status?kz=LU123456" -> "LU123456" (plates are sent normalized, no escaping needed)
    std::string urlArg(const std::string& pathAndQuery, const std::string& name)
    {
        size_t at = pathAndQuery.find(name + "=");
        return at == std::string::npos ? "" : pathAndQuery.substr(at + name.size() + 1);
    }

    class System : public KasseLink
    {
    public:
        RegistrationList registrations;
        SessionStore sessions;
        Barrier entryBarrier;
        Barrier exitBarrier;
        EventLog log;
        std::string lastEntryPlate;
        std::string lastExitPlate;
        int64_t now = START;
        uint32_t nowMs = 1000;
        bool kasseOn = true;

        System()
        {
            loadDefaultRegistrations(registrations);
            loadDefaultSessions(sessions);
        }

        HttpReply postJson(const char* path, const std::string& body) override
        {
            if (!kasseOn)
            {
                return {};
            }
            std::string p = path;
            ApiResponse r = p == "/api/entry"  ? handleEntry(sessions, body, now, true)
                            : p == "/api/exit" ? handleExit(sessions, body, now, true)
                                               : ApiResponse{404, "{\"error\":\"not_found\"}"};
            return {r.status, r.body};
        }

        HttpReply getJson(const std::string& pathAndQuery) override
        {
            if (!kasseOn)
            {
                return {};
            }
            ApiResponse r = handleStatus(sessions, urlArg(pathAndQuery, "kz"), now);
            return {r.status, r.body};
        }

        GateResult entry(const std::string& kz, const std::string& entryTime = "")
        {
            GateResult r = entryRequest(registrations, kz, entryTime, *this);
            if (r.open)
            {
                entryBarrier.open(nowMs);
            }
            lastEntryPlate = r.plate;
            logGateResult(log, GateLane::Entry, r, now);
            return r;
        }

        // Entry backdated by the given duration, typed into the camera form like a tester would.
        GateResult entryAgo(const std::string& kz, int64_t seconds)
        {
            return entry(kz, formatDateTime(now - seconds));
        }

        GateResult exit(const std::string& kz)
        {
            GateResult r = exitRequest(kz, *this);
            if (r.open)
            {
                exitBarrier.open(nowMs);
            }
            lastExitPlate = r.plate;
            logGateResult(log, GateLane::Exit, r, now);
            return r;
        }

        FormResult pay(const std::string& kz, const char* method, const char* amount = "")
        {
            return handlePayForm(sessions, kz, method, amount, now, true);
        }

        std::string page(Page p, const std::string& kz)
        {
            return renderPage(p, sessions, kz, Notice{}, now, true);
        }

        HttpReply status(const std::string& kz)
        {
            ApiResponse r = handleStatus(sessions, kz, now);
            return {r.status, r.body};
        }

        void wait(int64_t seconds, bool entryLoop = false, bool exitLoop = false)
        {
            for (int64_t i = 0; i < seconds; ++i)
            {
                now++;
                nowMs += 1000;
                logBarrierEvent(log, GateLane::Entry, entryBarrier.update(nowMs, entryLoop), now);
                logBarrierEvent(log, GateLane::Exit, exitBarrier.update(nowMs, exitLoop), now);
            }
        }

        // Operator «Schranke öffnen» → «Bestätigen».
        RemoteOpenResult remote(GateLane lane, const std::string& kz, const std::string& reason)
        {
            RemoteOpenResult r = remoteOpen(lane, kz, reason, now, *this, log);
            if (r.open)
            {
                (lane == GateLane::Entry ? entryBarrier : exitBarrier).open(nowMs);
            }
            return r;
        }

        // Advances time in 100 ms steps (closing takes 2 s, so a loop can hit it mid-sweep).
        void waitMs(uint32_t ms, bool entryLoop = false, bool exitLoop = false)
        {
            for (uint32_t t = 0; t < ms; t += 100)
            {
                nowMs += 100;
                logBarrierEvent(log, GateLane::Entry, entryBarrier.update(nowMs, entryLoop), now);
                logBarrierEvent(log, GateLane::Exit, exitBarrier.update(nowMs, exitLoop), now);
            }
        }

        size_t eventCount(EventType type) const
        {
            size_t n = 0;
            for (const Event& e : log.all())
            {
                n += e.type == type ? 1 : 0;
            }
            return n;
        }

        void rebootKasse()
        {
            std::string file = sessionsToJson(sessions);
            sessions = SessionStore();
            TEST_ASSERT_TRUE(sessionsFromJson(file, sessions));
        }

        void rebootSchranken()
        {
            std::string file = registrationsToJson(registrations);
            registrations = RegistrationList();
            TEST_ASSERT_TRUE(registrationsFromJson(file, registrations));
        }

        size_t sessionCount(const std::string& plate) const
        {
            size_t n = 0;
            for (const ParkingSession& s : sessions.all())
            {
                n += s.plate == plate ? 1 : 0;
            }
            return n;
        }
    };

    void assertOpen(const GateResult& r)
    {
        TEST_ASSERT_TRUE_MESSAGE(r.open, r.message.c_str());
    }

    void assertClosed(const GateResult& r, const char* message)
    {
        TEST_ASSERT_FALSE_MESSAGE(r.open, r.message.c_str());
        TEST_ASSERT_EQUAL_STRING(message, r.message.c_str());
    }
}

void setUp()
{
}

void tearDown()
{
}

void test_tf01_register_survives_reboot()
{
    System sys;
    FormResult r = handleRegistrationForm(sys.registrations, "Test Kunde C", "NW 654321");
    TEST_ASSERT_TRUE(contains(r.notice.text, "Kennzeichen registriert"));
    sys.rebootSchranken();
    TEST_ASSERT_TRUE(sys.registrations.contains("NW654321"));
    TEST_ASSERT_TRUE(contains(renderRegistrationPage(sys.registrations, Notice{}), "<td>NW654321</td>"));
}

void test_tf02_invalid_plates_not_stored()
{
    System sys;
    for (const char* kz : {"", "LU-12#45", "LU 1234567", "L 123"})
    {
        FormResult r = handleRegistrationForm(sys.registrations, "Test Kunde C", kz);
        TEST_ASSERT_FALSE(r.changed);
        TEST_ASSERT_EQUAL_STRING("err", r.notice.cls);
    }
    TEST_ASSERT_EQUAL_UINT(4, sys.registrations.all().size());
    TEST_ASSERT_TRUE(handleRegistrationForm(sys.registrations, "Test Kunde C", "UR 1").changed);
    TEST_ASSERT_TRUE(sys.registrations.contains("UR1"));
}

void test_tf03_duplicate_plate_rejected()
{
    System sys;
    for (const char* kz : {"LU 123456", "lu123456"})
    {
        TEST_ASSERT_EQUAL_STRING("Kennzeichen bereits registriert",
                                 handleRegistrationForm(sys.registrations, "Test Kunde B", kz).notice.text.c_str());
    }
    TEST_ASSERT_EQUAL_UINT(4, sys.registrations.all().size());
    TEST_ASSERT_EQUAL_STRING("Test Kunde A", sys.registrations.all()[0].name.c_str());
}

void test_tf04_entry_opens_and_closes_after_loop_released()
{
    System sys;
    assertOpen(sys.entry("LU123456"));
    TEST_ASSERT_EQUAL(BarrierState::Open, sys.entryBarrier.state());
    sys.wait(2);
    sys.wait(10, true); // car on the loop
    TEST_ASSERT_EQUAL(BarrierState::Open, sys.entryBarrier.state());
    sys.wait(1);
    TEST_ASSERT_EQUAL(BarrierState::Closing, sys.entryBarrier.state());
    sys.wait(BARRIER_CLOSE_MS / 1000);
    TEST_ASSERT_EQUAL(BarrierState::Closed, sys.entryBarrier.state());
    TEST_ASSERT_EQUAL(BarrierState::Closed, sys.exitBarrier.state());
}

void test_tf06_normalized_plate_matches_at_exit()
{
    System sys;
    assertOpen(sys.entry("lu 123 456"));
    sys.wait(80 * MIN);
    TEST_ASSERT_EQUAL_STRING("ok", sys.pay("LU123456", "card_ok").notice.cls);
    assertOpen(sys.exit(" LU123456 "));
    TEST_ASSERT_EQUAL_UINT(1, sys.sessionCount("LU123456"));
    TEST_ASSERT_EQUAL(SessionStatus::Closed, sys.sessions.findLatest("LU123456")->status);
}

void test_tf07_unregistered_stays_closed_no_session()
{
    System sys;
    assertClosed(sys.entry("BE 4711"), "BE4711 – nicht registriert, bitte Ruftaste drücken");
    sys.wait(10);
    TEST_ASSERT_EQUAL(BarrierState::Closed, sys.entryBarrier.state());
    TEST_ASSERT_EQUAL_UINT(0, sys.sessions.all().size());
    TEST_ASSERT_TRUE(contains(sys.page(Page::Kasse, "BE4711"), "Kein offener Parkvorgang für BE4711"));
}

void test_tf08_ten_entry_cycles()
{
    System sys;
    for (int i = 0; i < 10; ++i)
    {
        const char* kz = i % 2 == 0 ? "LU123456" : "ZH98765";
        assertOpen(sys.entry(kz));
        sys.wait(30 * MIN);
        TEST_ASSERT_EQUAL_STRING("ok", sys.pay(kz, "twint").notice.cls);
        assertOpen(sys.exit(kz));
        sys.wait(10);
    }
    TEST_ASSERT_EQUAL_UINT(10, sys.sessions.all().size());
}

void test_tf09_entry_time_and_running_duration()
{
    System sys;
    assertOpen(sys.entry("ZH 98765"));
    const ParkingSession* s = sys.sessions.findActive("ZH98765");
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT64(START, s->entry);
    std::string html = sys.page(Page::Kasse, "ZH98765");
    TEST_ASSERT_TRUE(contains(html, "<td>15.10.2025 14:06:40</td>"));
    TEST_ASSERT_TRUE(contains(html, "<td>0 h 0 min</td>"));
    sys.wait(90);
    TEST_ASSERT_TRUE(contains(sys.page(Page::Kasse, "ZH98765"), "<td>0 h 1 min</td>"));
}

void test_tf10_second_entry_rejected()
{
    System sys;
    assertOpen(sys.entry("ZH98765"));
    sys.wait(5 * MIN);
    assertClosed(sys.entry("ZH 98765"), "Parkvorgang bereits offen – bitte Ruftaste drücken");
    TEST_ASSERT_EQUAL_UINT(1, sys.sessionCount("ZH98765"));
    TEST_ASSERT_EQUAL_INT64(START, sys.sessions.findActive("ZH98765")->entry);
}

void test_tf11_kasse_off_at_entry()
{
    System sys;
    sys.kasseOn = false;
    assertClosed(sys.entry("LU123456"), "Kasse nicht erreichbar – bitte Ruftaste drücken");
    TEST_ASSERT_EQUAL(BarrierState::Closed, sys.entryBarrier.state());
    TEST_ASSERT_EQUAL_UINT(0, sys.sessions.all().size());
    sys.wait(2 * MIN);
    sys.kasseOn = true;
    assertOpen(sys.entry("LU123456"));
    TEST_ASSERT_EQUAL_INT64(START + 2 * MIN, sys.sessions.findActive("LU123456")->entry);
}

void test_tf18_reference_fees_via_backdated_entry()
{
    struct Case
    {
        const char* id;
        int64_t ago;
        const char* fee;
    };
    const Case cases[] = {
        {"R-01", 0, "CHF 0.00"},
        {"R-02", 14 * MIN, "CHF 0.00"},
        {"R-03", 16 * MIN, "CHF 2.00"},
        {"R-04", 59 * MIN, "CHF 2.00"},
        {"R-05", HOUR + MIN, "CHF 4.00"},
        {"R-06", 2 * HOUR + 30 * MIN, "CHF 6.00"},
        {"R-07", 9 * HOUR + 59 * MIN, "CHF 20.00"},
        {"R-08", 10 * HOUR + MIN, "CHF 20.00"},
        {"R-09", 23 * HOUR + 59 * MIN, "CHF 20.00"},
        {"R-10", 24 * HOUR + MIN, "CHF 22.00"},
        {"R-11", 47 * HOUR + 59 * MIN, "CHF 40.00"},
    };
    for (const Case& c : cases)
    {
        System sys;
        GateResult r = c.ago == 0 ? sys.entry("LU123456") : sys.entryAgo("LU123456", c.ago);
        TEST_ASSERT_TRUE_MESSAGE(r.open, c.id);
        sys.wait(30); // read at the Kasse within 60 s
        std::string html = sys.page(Page::Kasse, "LU123456");
        TEST_ASSERT_TRUE_MESSAGE(contains(html, std::string("<th>Gebühr</th><td>") + c.fee + "</td>"), c.id);
    }

    System sys; // R-12: entry time 1 h in the future
    GateResult r = sys.entryAgo("LU123456", -HOUR);
    assertClosed(r, "Einfahrtszeit liegt in der Zukunft");
    TEST_ASSERT_EQUAL_UINT(0, sys.sessions.all().size());
}

void test_tf19_duration_and_fee_at_kasse_and_automat()
{
    System sys;
    assertOpen(sys.entryAgo("LU123456", 80 * MIN));
    for (Page p : {Page::Kasse, Page::Automat})
    {
        std::string html = sys.page(p, "LU123456");
        TEST_ASSERT_TRUE(contains(html, "<td>1 h 20 min</td>"));
        TEST_ASSERT_TRUE(contains(html, "<td>CHF 4.00</td>"));
        TEST_ASSERT_TRUE(contains(html, "2 angefangene Stunden à CHF 2.00 ab Einfahrt (15 Freiminuten überschritten)"));
    }
}

void test_tf20_card_payment_once()
{
    System sys;
    assertOpen(sys.entryAgo("LU123456", 80 * MIN));
    FormResult paid = sys.pay("LU123456", "card_ok");
    TEST_ASSERT_EQUAL_STRING("Bezahlt – CHF 4.00, Karte – Transaktion TX-000001", paid.notice.text.c_str());
    TEST_ASSERT_TRUE(contains(sys.status("LU123456").body, "\"status\":\"bezahlt\""));
    TEST_ASSERT_EQUAL_STRING("bereits bezahlt", sys.pay("LU123456", "card_ok").notice.text.c_str());
    TEST_ASSERT_EQUAL_UINT32(2, sys.sessions.nextTxNumber());
}

void test_tf21_automat_cash_and_twint()
{
    System sys;
    assertOpen(sys.entryAgo("LU123456", 2 * HOUR + 30 * MIN));
    assertOpen(sys.entryAgo("ZH98765", 2 * HOUR + 30 * MIN));
    assertOpen(sys.entryAgo("AG1", 30 * MIN));

    TEST_ASSERT_TRUE(contains(sys.pay("LU123456", "cash", "10.00").notice.text, "Rückgeld CHF 4.00"));
    TEST_ASSERT_TRUE(contains(sys.status("LU123456").body, "\"status\":\"bezahlt\""));
    TEST_ASSERT_TRUE(contains(sys.pay("ZH98765", "twint").notice.text, "Bezahlt – CHF 6.00, TWINT"));
    TEST_ASSERT_TRUE(contains(sys.status("ZH98765").body, "\"status\":\"bezahlt\""));

    TEST_ASSERT_EQUAL_STRING("Restbetrag CHF 1.00 – unbezahlt", sys.pay("AG1", "cash", "1.00").notice.text.c_str());
    TEST_ASSERT_TRUE(contains(sys.status("AG1").body, "\"status\":\"unbezahlt\""));
    TEST_ASSERT_TRUE(contains(sys.pay("AG1", "cash", "1.00").notice.text, "Rückgeld CHF 0.00"));
    TEST_ASSERT_TRUE(contains(sys.status("AG1").body, "\"status\":\"bezahlt\""));
}

void test_tf22_automat_without_session()
{
    System sys;
    std::string html = sys.page(Page::Automat, "SZ 555");
    TEST_ASSERT_TRUE(contains(html, "Kein offener Parkvorgang für SZ555"));
    TEST_ASSERT_FALSE(contains(html, "/automat/pay"));
    TEST_ASSERT_EQUAL_STRING("Kein offener Parkvorgang für SZ555", sys.pay("SZ555", "card_ok").notice.text.c_str());
    TEST_ASSERT_EQUAL_UINT32(1, sys.sessions.nextTxNumber());
}

void test_tf23_paid_exit_closes_session()
{
    System sys;
    assertOpen(sys.entryAgo("LU123456", 80 * MIN));
    sys.pay("LU123456", "card_ok");
    sys.wait(MIN);
    int64_t exitTime = sys.now;
    assertOpen(sys.exit("LU123456"));
    TEST_ASSERT_EQUAL(BarrierState::Open, sys.exitBarrier.state());
    const ParkingSession* s = sys.sessions.findLatest("LU123456");
    TEST_ASSERT_EQUAL(SessionStatus::Closed, s->status);
    TEST_ASSERT_EQUAL_INT64(exitTime, s->exit);
    std::string before = sys.page(Page::Kasse, "LU123456");
    TEST_ASSERT_TRUE(contains(before, "<td>abgeschlossen</td>"));
    TEST_ASSERT_TRUE(contains(before, "<td>1 h 21 min</td>"));
    sys.wait((BARRIER_MIN_OPEN_MS + BARRIER_CLOSE_MS) / 1000);
    TEST_ASSERT_EQUAL(BarrierState::Closed, sys.exitBarrier.state());
    sys.wait(MIN);
    TEST_ASSERT_TRUE(contains(sys.page(Page::Kasse, "LU123456"), "<td>1 h 21 min</td>"));
}

void test_tf24_unpaid_exit_then_pay_then_exit()
{
    System sys;
    assertOpen(sys.entryAgo("ZH98765", 2 * HOUR + 30 * MIN));
    assertClosed(sys.exit("ZH98765"), "Gebühr CHF 6.00 nicht bezahlt – bitte am Automaten bezahlen");
    sys.wait(10);
    TEST_ASSERT_EQUAL(BarrierState::Closed, sys.exitBarrier.state());
    TEST_ASSERT_EQUAL(SessionStatus::Open, sys.sessions.findActive("ZH98765")->status);
    TEST_ASSERT_EQUAL_STRING("ok", sys.pay("ZH98765", "twint").notice.cls);
    assertOpen(sys.exit("ZH98765"));
}

void test_tf25_free_time_exit()
{
    System sys;
    assertOpen(sys.entryAgo("AG1", 5 * MIN));
    assertOpen(sys.exit("AG 1"));
    const ParkingSession* s = sys.sessions.findLatest("AG1");
    TEST_ASSERT_EQUAL(SessionStatus::ClosedFree, s->status);
    TEST_ASSERT_EQUAL_UINT32(0, s->amountRappen);
    std::string html = sys.page(Page::Kasse, "AG1");
    TEST_ASSERT_TRUE(contains(html, "<td>CHF 0.00</td>"));
    TEST_ASSERT_TRUE(contains(html, "abgeschlossen (Freizeit)"));
}

void test_tf29_status_interface_twenty_requests()
{
    System sys;
    assertOpen(sys.entryAgo("LU123456", 80 * MIN));
    sys.pay("LU123456", "card_ok");
    assertOpen(sys.entryAgo("ZH98765", 2 * HOUR + 30 * MIN));
    struct Case
    {
        const char* kz;
        int status;
        const char* part;
    };
    const Case cases[] = {
        {"LU123456", 200, "\"status\":\"bezahlt\""},
        {"ZH 98765", 200, "\"status\":\"unbezahlt\""},
        {"SZ555", 200, "\"status\":\"unbekannt\""},
        {"", 400, "invalid_plate"},
        {"LU-12#45", 400, "invalid_plate"},
    };
    int requests = 0;
    for (int round = 0; round < 4; ++round)
    {
        for (const Case& c : cases)
        {
            HttpReply r = sys.status(c.kz);
            TEST_ASSERT_EQUAL_INT_MESSAGE(c.status, r.status, c.kz);
            TEST_ASSERT_TRUE_MESSAGE(contains(r.body, c.part), r.body.c_str());
            requests++;
        }
    }
    TEST_ASSERT_EQUAL_INT(20, requests);
}

void test_tf30_kasse_off_at_exit_paid_status_persisted()
{
    System sys;
    assertOpen(sys.entryAgo("LU123456", 80 * MIN));
    sys.pay("LU123456", "card_ok");
    assertOpen(sys.entryAgo("ZH98765", 2 * HOUR + 30 * MIN));
    sys.kasseOn = false;
    assertClosed(sys.exit("ZH98765"), "Kasse nicht erreichbar – bitte Ruftaste drücken");
    assertClosed(sys.exit("LU123456"), "Kasse nicht erreichbar – bitte Ruftaste drücken");
    TEST_ASSERT_EQUAL(BarrierState::Closed, sys.exitBarrier.state());
    sys.rebootKasse();
    sys.kasseOn = true;
    TEST_ASSERT_EQUAL(SessionStatus::Paid, sys.sessions.findActive("LU123456")->status);
    assertOpen(sys.exit("LU123456"));
    assertClosed(sys.exit("ZH98765"), "Gebühr CHF 6.00 nicht bezahlt – bitte am Automaten bezahlen");
}

void test_tf32_whole_system()
{
    System sys;
    TEST_ASSERT_TRUE(handleRegistrationForm(sys.registrations, "Test Kunde F", "OW 2026").changed);
    assertOpen(sys.entryAgo("OW 2026", 2 * HOUR + 30 * MIN));
    TEST_ASSERT_TRUE(contains(sys.page(Page::Automat, "OW2026"), "<td>CHF 6.00</td>"));
    assertClosed(sys.exit("OW2026"), "Gebühr CHF 6.00 nicht bezahlt – bitte am Automaten bezahlen");
    TEST_ASSERT_TRUE(contains(sys.pay("OW2026", "cash", "10.00").notice.text, "Rückgeld CHF 4.00"));
    assertOpen(sys.exit("OW2026"));
    TEST_ASSERT_EQUAL(SessionStatus::Closed, sys.sessions.findLatest("OW2026")->status);
}

void test_tf14_six_safety_stops_logged()
{
    System sys;
    for (int round = 0; round < 3; ++round)
    {
        for (GateLane lane : {GateLane::Entry, GateLane::Exit})
        {
            bool entry = lane == GateLane::Entry;
            (entry ? sys.entryBarrier : sys.exitBarrier).open(sys.nowMs);
            sys.waitMs(BARRIER_MIN_OPEN_MS + 1000); // 1 s into the closing sweep
            TEST_ASSERT_EQUAL(BarrierState::Closing, (entry ? sys.entryBarrier : sys.exitBarrier).state());
            sys.waitMs(100, entry, !entry);
            TEST_ASSERT_EQUAL(BarrierState::Open, (entry ? sys.entryBarrier : sys.exitBarrier).state());
            sys.waitMs(BARRIER_MIN_OPEN_MS + BARRIER_CLOSE_MS + 100);
            TEST_ASSERT_EQUAL(BarrierState::Closed, (entry ? sys.entryBarrier : sys.exitBarrier).state());
        }
    }
    TEST_ASSERT_EQUAL_UINT(6, sys.eventCount(EventType::SafetyStop));
    std::string html = renderOperatorPage(OperatorLane{}, OperatorLane{GateLane::Exit}, sys.log, "safety_stop", Notice{});
    TEST_ASSERT_TRUE(contains(html, "6 Einträge"));
    TEST_ASSERT_TRUE(contains(html, "<td>Sicherheitsabschaltung</td><td>Einfahrt</td>"));
    TEST_ASSERT_TRUE(contains(html, "<td>Sicherheitsabschaltung</td><td>Ausfahrt</td>"));
}

void test_tf15_loop_fault_logged_once()
{
    System sys;
    sys.entryBarrier.open(sys.nowMs);
    sys.wait(4 * MIN + 50, true);
    TEST_ASSERT_EQUAL_UINT(0, sys.eventCount(EventType::LoopFault));
    sys.wait(20, true);
    TEST_ASSERT_EQUAL_UINT(1, sys.eventCount(EventType::LoopFault));
    TEST_ASSERT_EQUAL(BarrierState::Open, sys.entryBarrier.state());
    TEST_ASSERT_EQUAL_STRING("Sicherungsschleife Einfahrt länger als 5 min belegt", sys.log.all().back().note.c_str());
}

void test_tf16_remote_opening_at_entry()
{
    System sys;
    assertClosed(sys.entry("BE 4711"), "BE4711 – nicht registriert, bitte Ruftaste drücken");
    TEST_ASSERT_EQUAL_STRING("BE4711", sys.lastEntryPlate.c_str());
    TEST_ASSERT_EQUAL_STRING("kein Parkvorgang", laneStatus(sys, sys.lastEntryPlate).c_str());

    RemoteOpenResult r = sys.remote(GateLane::Entry, sys.lastEntryPlate, "Kennzeichen nicht erkannt");
    TEST_ASSERT_TRUE(r.open);
    TEST_ASSERT_EQUAL(BarrierState::Open, sys.entryBarrier.state());
    TEST_ASSERT_NOT_NULL(sys.sessions.findActive("BE4711"));
    const Event& e = sys.log.all().back();
    TEST_ASSERT_EQUAL(EventType::RemoteOpening, e.type);
    TEST_ASSERT_EQUAL_STRING("betrieb", e.user.c_str());
    TEST_ASSERT_EQUAL(GateLane::Entry, e.lane);
    TEST_ASSERT_EQUAL_INT64(sys.now, e.time);
    TEST_ASSERT_EQUAL_STRING("Kennzeichen nicht erkannt", e.reason.c_str());
}

void test_tf17_remote_opening_needs_reason()
{
    System sys;
    sys.entry("BE4711");
    RemoteOpenResult r = sys.remote(GateLane::Entry, "BE4711", "");
    TEST_ASSERT_FALSE(r.open);
    TEST_ASSERT_EQUAL_STRING("Bitte Grund auswählen", r.notice.text.c_str());
    TEST_ASSERT_EQUAL(BarrierState::Closed, sys.entryBarrier.state());
    TEST_ASSERT_EQUAL_UINT(0, sys.log.all().size());
}

void test_tf30_fault_logged()
{
    System sys;
    sys.kasseOn = false;
    sys.exit("ZH98765");
    sys.exit("LU123456");
    TEST_ASSERT_EQUAL_UINT(2, sys.eventCount(EventType::KasseUnreachable));
    TEST_ASSERT_EQUAL(GateLane::Exit, sys.log.all()[0].lane);
}

void test_remote_opening_with_kasse_off_stays_closed()
{
    System sys;
    assertOpen(sys.entryAgo("ZH98765", 2 * HOUR + 30 * MIN));
    sys.kasseOn = false;
    RemoteOpenResult r = sys.remote(GateLane::Exit, "ZH98765", "Kulanz");
    TEST_ASSERT_FALSE(r.open);
    TEST_ASSERT_EQUAL(BarrierState::Closed, sys.exitBarrier.state());
    TEST_ASSERT_EQUAL_UINT(0, sys.eventCount(EventType::RemoteOpening));
    TEST_ASSERT_EQUAL_UINT(1, sys.eventCount(EventType::KasseUnreachable));
    sys.kasseOn = true;
    TEST_ASSERT_EQUAL(SessionStatus::Open, sys.sessions.findActive("ZH98765")->status);
}

void test_tf31_remote_opening_at_exit_without_payment()
{
    System sys;
    assertOpen(sys.entryAgo("ZH98765", 2 * HOUR + 30 * MIN));
    assertClosed(sys.exit("ZH98765"), "Gebühr CHF 6.00 nicht bezahlt – bitte am Automaten bezahlen");
    TEST_ASSERT_EQUAL_STRING("unbezahlt (CHF 6.00)", laneStatus(sys, sys.lastExitPlate).c_str());

    RemoteOpenResult r = sys.remote(GateLane::Exit, sys.lastExitPlate, "Kulanz");
    TEST_ASSERT_TRUE(r.open);
    TEST_ASSERT_EQUAL(BarrierState::Open, sys.exitBarrier.state());
    const Event& e = sys.log.all().back();
    TEST_ASSERT_EQUAL_STRING("betrieb", e.user.c_str());
    TEST_ASSERT_EQUAL(GateLane::Exit, e.lane);
    TEST_ASSERT_EQUAL_STRING("ZH98765", e.plate.c_str());
    TEST_ASSERT_EQUAL_STRING("Kulanz", e.reason.c_str());
    TEST_ASSERT_EQUAL_STRING("ohne Zahlung", e.note.c_str());
    TEST_ASSERT_TRUE(contains(sys.page(Page::Kasse, "ZH98765"), "<td>ohne Zahlung ausgefahren</td>"));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_tf01_register_survives_reboot);
    RUN_TEST(test_tf02_invalid_plates_not_stored);
    RUN_TEST(test_tf03_duplicate_plate_rejected);
    RUN_TEST(test_tf04_entry_opens_and_closes_after_loop_released);
    RUN_TEST(test_tf06_normalized_plate_matches_at_exit);
    RUN_TEST(test_tf07_unregistered_stays_closed_no_session);
    RUN_TEST(test_tf08_ten_entry_cycles);
    RUN_TEST(test_tf09_entry_time_and_running_duration);
    RUN_TEST(test_tf10_second_entry_rejected);
    RUN_TEST(test_tf11_kasse_off_at_entry);
    RUN_TEST(test_tf18_reference_fees_via_backdated_entry);
    RUN_TEST(test_tf19_duration_and_fee_at_kasse_and_automat);
    RUN_TEST(test_tf20_card_payment_once);
    RUN_TEST(test_tf21_automat_cash_and_twint);
    RUN_TEST(test_tf22_automat_without_session);
    RUN_TEST(test_tf23_paid_exit_closes_session);
    RUN_TEST(test_tf24_unpaid_exit_then_pay_then_exit);
    RUN_TEST(test_tf25_free_time_exit);
    RUN_TEST(test_tf29_status_interface_twenty_requests);
    RUN_TEST(test_tf30_kasse_off_at_exit_paid_status_persisted);
    RUN_TEST(test_tf32_whole_system);
    RUN_TEST(test_tf14_six_safety_stops_logged);
    RUN_TEST(test_tf15_loop_fault_logged_once);
    RUN_TEST(test_tf16_remote_opening_at_entry);
    RUN_TEST(test_tf17_remote_opening_needs_reason);
    RUN_TEST(test_tf30_fault_logged);
    RUN_TEST(test_tf31_remote_opening_at_exit_without_payment);
    RUN_TEST(test_remote_opening_with_kasse_off_stays_closed);
    return UNITY_END();
}
