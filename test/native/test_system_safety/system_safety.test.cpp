#include <unity.h>

#include "operator_view.h"
#include "system_rig.h"

// System test, Testkonzept 8.3 barrier safety and 8.4 remote opening (TF-12..TF-17), plus the call button and
// «Durchfahren». The safety loop is «Fahrzeug bleibt stehen» / «Fahrzeug fährt weiter» on the camera page.

using namespace fixtures;

namespace
{
    // Opens the lane's barrier for a registered car and lets it drive through; returns when closing starts.
    void openAndStartClosing(SystemRig& sys, GateLane lane)
    {
        if (lane == GateLane::Entry)
        {
            sys.entry(KZ_02);
        }
        else
        {
            sys.login();
            sys.remoteOpen(GateLane::Exit, KZ_02, "Kulanz");
        }
        sys.driveThrough(lane);
        sys.waitMs(BARRIER_MIN_OPEN_MS + 1000); // 1 s into the closing sweep
        TEST_ASSERT_TRUE(BarrierState::Closing == sys.state(lane));
    }
}

void setUp()
{
}

void tearDown()
{
}

void test_tf12_stays_open_while_loop_occupied()
{
    for (GateLane lane : {GateLane::Entry, GateLane::Exit})
    {
        SystemRig sys;
        sys.entry(KZ_02);
        if (lane == GateLane::Exit)
        {
            sys.driveThrough(GateLane::Entry);
            sys.wait(2 * HOUR);
            sys.automatPay(KZ_02, "twint");
            sys.exit(KZ_02);
        }
        TEST_ASSERT_TRUE(BarrierState::Open == sys.state(lane));
        sys.hold(lane, true);
        sys.wait(30);
        TEST_ASSERT_TRUE(BarrierState::Open == sys.state(lane));
        TEST_ASSERT_TRUE(contains(sys.page("/kamera"), "Sicherungsschleife: <strong>belegt</strong>"));
        sys.hold(lane, false);
        sys.wait((BARRIER_CLOSE_MS / 1000) + 1);
        TEST_ASSERT_TRUE(BarrierState::Closed == sys.state(lane));
    }
}

void test_tf13_tf14_stop_while_closing_and_six_log_entries()
{
    SystemRig sys;
    for (int round = 0; round < 3; ++round)
    {
        for (GateLane lane : {GateLane::Entry, GateLane::Exit})
        {
            openAndStartClosing(sys, lane);
            sys.hold(lane, true);
            sys.waitMs(100); // stops in the next update (TF-13: < 0.5 s)
            TEST_ASSERT_TRUE(BarrierState::Open == sys.state(lane));
            sys.hold(lane, false);
            sys.waitMs(BARRIER_MIN_OPEN_MS + BARRIER_CLOSE_MS + 100);
            TEST_ASSERT_TRUE(BarrierState::Closed == sys.state(lane));
            if (lane == GateLane::Entry)
            {
                sys.wait(80 * MIN);
                sys.automatPay(KZ_02, "twint");
                sys.exit(KZ_02); // car leaves, so it can enter again next round
                sys.driveThrough(GateLane::Exit);
                sys.waitMs(BARRIER_MIN_OPEN_MS + BARRIER_CLOSE_MS + 100);
            }
        }
    }
    TEST_ASSERT_EQUAL_UINT(6, sys.eventCount(EventType::SafetyStop));
    sys.login();
    std::string log = sys.page("/betrieb", {{"filter", "safety_stop"}});
    TEST_ASSERT_TRUE(contains(log, "6 Einträge"));
    TEST_ASSERT_TRUE(contains(log, "<td>Sicherheitsabschaltung</td><td>Einfahrt</td>"));
    TEST_ASSERT_TRUE(contains(log, "<td>Sicherheitsabschaltung</td><td>Ausfahrt</td>"));
}

void test_tf15_loop_fault_after_five_minutes()
{
    SystemRig sys;
    sys.entry(KZ_01);
    sys.hold(GateLane::Entry, true);
    sys.wait(4 * MIN + 50);
    TEST_ASSERT_EQUAL_UINT(0, sys.eventCount(EventType::LoopFault));
    TEST_ASSERT_FALSE(contains(sys.page("/kamera"), "länger als 5 min belegt"));
    sys.wait(20);
    TEST_ASSERT_TRUE(contains(sys.page("/kamera"), "Sicherungsschleife Einfahrt länger als 5 min belegt"));
    TEST_ASSERT_EQUAL_UINT(1, sys.eventCount(EventType::LoopFault));
    int64_t logged = sys.app().events().all().back().time;
    TEST_ASSERT_TRUE(logged >= NOW + 5 * MIN && logged <= NOW + 5 * MIN + 2); // ± 2 s as in the Testkonzept
    TEST_ASSERT_TRUE(BarrierState::Open == sys.state(GateLane::Entry));
}

void test_tf16_remote_opening_at_entry()
{
    SystemRig sys;
    sys.entry(KZ_03);
    sys.callButton(GateLane::Entry);
    sys.login();
    std::string lanes = sys.page("/betrieb");
    TEST_ASSERT_TRUE(contains(lanes, "Einfahrt"));
    TEST_ASSERT_TRUE(contains(lanes, "BE4711"));
    TEST_ASSERT_TRUE(contains(lanes, "kein Parkvorgang"));
    TEST_ASSERT_TRUE(contains(lanes, "Ruftaste gedrückt – BE4711"));

    std::string after = sys.remoteOpen(GateLane::Entry, "BE4711", "Kennzeichen nicht erkannt");
    TEST_ASSERT_TRUE(contains(after, "Schranke Einfahrt geöffnet"));
    TEST_ASSERT_TRUE(BarrierState::Open == sys.state(GateLane::Entry));
    TEST_ASSERT_NOT_NULL(sys.sessions().findActive("BE4711"));
    const Event& e = sys.app().events().all().back();
    TEST_ASSERT_TRUE(EventType::RemoteOpening == e.type);
    TEST_ASSERT_EQUAL_STRING("betrieb", e.user.c_str());
    TEST_ASSERT_TRUE(GateLane::Entry == e.lane);
    TEST_ASSERT_EQUAL_INT64(sys.now, e.time);
    TEST_ASSERT_EQUAL_STRING("Kennzeichen nicht erkannt", e.reason.c_str());
    TEST_ASSERT_FALSE(contains(sys.page("/betrieb"), "Ruftaste gedrückt – BE4711")); // call handled
}

void test_tf17_remote_opening_needs_reason()
{
    SystemRig sys;
    sys.login();
    TEST_ASSERT_TRUE(contains(sys.remoteOpen(GateLane::Entry, "BE4711", ""), "Bitte Grund auswählen"));
    TEST_ASSERT_TRUE(BarrierState::Closed == sys.state(GateLane::Entry));
    TEST_ASSERT_EQUAL_UINT(0, sys.app().events().all().size());
}

void test_remote_opening_with_kasse_off_stays_closed()
{
    SystemRig sys;
    sys.entryAgo(KZ_02, 2 * HOUR + 30 * MIN);
    sys.login();
    sys.kasse.online = false;
    TEST_ASSERT_TRUE(contains(sys.remoteOpen(GateLane::Exit, KZ_02, "Kulanz"), "Kasse nicht erreichbar – Schranke bleibt zu"));
    TEST_ASSERT_TRUE(BarrierState::Closed == sys.state(GateLane::Exit));
    TEST_ASSERT_EQUAL_UINT(0, sys.eventCount(EventType::RemoteOpening));
    TEST_ASSERT_EQUAL_UINT(1, sys.eventCount(EventType::KasseUnreachable));
    TEST_ASSERT_TRUE(SessionStatus::Open == sys.sessions().findActive("ZH98765")->status);
}

void test_new_customer_by_operator_then_automat_and_exit()
{
    SystemRig sys;
    sys.entry(KZ_12);
    sys.login();
    std::string after = sys.remoteOpen(GateLane::Entry, KZ_12, NEW_CUSTOMER_REASON, "Test Kunde C");
    TEST_ASSERT_TRUE(contains(after, "Neukunde registriert"));
    TEST_ASSERT_TRUE(BarrierState::Open == sys.state(GateLane::Entry));
    TEST_ASSERT_TRUE(sys.app().registrations().contains("NW654321"));
    sys.driveThrough(GateLane::Entry);
    sys.wait(2 * HOUR + 30 * MIN);
    TEST_ASSERT_EQUAL_STRING("Gebühr CHF 6.00 nicht bezahlt – bitte am Automaten bezahlen", sys.exit(KZ_12).c_str());
    TEST_ASSERT_TRUE(contains(sys.automatPay("NW654321", "cash", "10"), "Rückgeld CHF 4.00"));
    TEST_ASSERT_TRUE(contains(sys.exit(KZ_12), "Gute Fahrt"));
    sys.driveThrough(GateLane::Exit);
    sys.wait(10);
    TEST_ASSERT_TRUE(contains(sys.entry(KZ_12), "Willkommen")); // next time without the operator
}

void test_drive_through_closes_and_reports()
{
    SystemRig sys;
    sys.entry(KZ_01);
    sys.wait(60);
    TEST_ASSERT_TRUE(BarrierState::Open == sys.state(GateLane::Entry)); // no pass, no closing
    TEST_ASSERT_EQUAL_STRING("LU123456 durchgefahren – Schranke schliesst", sys.driveThrough(GateLane::Entry).c_str());
    sys.wait(BARRIER_CLOSE_MS / 1000 + 1);
    TEST_ASSERT_TRUE(BarrierState::Closed == sys.state(GateLane::Entry));
    TEST_ASSERT_TRUE(contains(sys.page("/kamera"), "LU123456 durchgefahren – Schranke geschlossen"));
    TEST_ASSERT_EQUAL_STRING("Keine offene Schranke – Fahrzeug kann nicht durchfahren",
                             sys.driveThrough(GateLane::Entry).c_str());
}

void test_call_button_reaches_operator()
{
    SystemRig sys;
    sys.entry(KZ_03);
    TEST_ASSERT_EQUAL_STRING("Leitstelle wurde gerufen – bitte warten", sys.callButton(GateLane::Entry).c_str());
    TEST_ASSERT_EQUAL_UINT(1, sys.eventCount(EventType::CallButton));
    sys.login();
    TEST_ASSERT_TRUE(contains(sys.page("/betrieb"), "Ruftaste gedrückt – BE4711 (15.10.2025 14:06:40)"));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_tf12_stays_open_while_loop_occupied);
    RUN_TEST(test_tf13_tf14_stop_while_closing_and_six_log_entries);
    RUN_TEST(test_tf15_loop_fault_after_five_minutes);
    RUN_TEST(test_tf16_remote_opening_at_entry);
    RUN_TEST(test_tf17_remote_opening_needs_reason);
    RUN_TEST(test_remote_opening_with_kasse_off_stays_closed);
    RUN_TEST(test_new_customer_by_operator_then_automat_and_exit);
    RUN_TEST(test_drive_through_closes_and_reports);
    RUN_TEST(test_call_button_reaches_operator);
    return UNITY_END();
}
