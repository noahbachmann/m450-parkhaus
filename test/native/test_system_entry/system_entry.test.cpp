#include <unity.h>

#include "system_rig.h"

// System test, Testkonzept 8.1 registration and 8.2 entry + parking session (TF-01..TF-11), through the pages of
// Pico 2 and the API of Pico 1 (see system_rig.h).

using namespace fixtures;

void setUp()
{
}

void tearDown()
{
}

void test_tf01_register_survives_reboot()
{
    SystemRig sys;
    sys.login();
    TEST_ASSERT_TRUE(contains(sys.submit("/registrierung/add", {{"name", "Test Kunde C"}, {"kz", KZ_12}}),
                              "Kennzeichen registriert: NW654321"));
    sys.rebootSchranken();
    sys.login();
    TEST_ASSERT_TRUE(contains(sys.page("/registrierung"), "<td>NW654321</td>"));
}

void test_tf02_invalid_plates_not_stored()
{
    SystemRig sys;
    sys.login();
    TEST_ASSERT_TRUE(contains(sys.submit("/registrierung/add", {{"name", "Test Kunde C"}, {"kz", KZ_05}}),
                              "Ungültiges Kennzeichen – erwartet 2 Buchstaben und 1–6 Ziffern"));
    for (const char* kz : {KZ_06, KZ_07, KZ_09})
    {
        std::string page = sys.submit("/registrierung/add", {{"name", "Test Kunde C"}, {"kz", kz}});
        TEST_ASSERT_TRUE_MESSAGE(contains(page, "Ungültiges Kennzeichen – erwartet 2 Buchstaben und 1–6 Ziffern"), kz);
    }
    TEST_ASSERT_EQUAL_UINT(4, sys.app().registrations().all().size());
    TEST_ASSERT_TRUE(contains(sys.submit("/registrierung/add", {{"name", "Test Kunde C"}, {"kz", KZ_13}}),
                              "Kennzeichen registriert: UR1"));
    TEST_ASSERT_TRUE(sys.app().registrations().contains("UR1"));
}

void test_tf03_duplicate_plate_rejected()
{
    SystemRig sys;
    sys.login();
    for (const char* kz : {"LU 123456", "lu123456"})
    {
        TEST_ASSERT_TRUE(contains(sys.submit("/registrierung/add", {{"name", "Test Kunde B"}, {"kz", kz}}),
                                  "Kennzeichen bereits registriert"));
    }
    TEST_ASSERT_EQUAL_UINT(4, sys.app().registrations().all().size());
    const Registration& a = sys.app().registrations().all()[0];
    TEST_ASSERT_EQUAL_STRING("LU123456", a.plate.c_str());
    TEST_ASSERT_EQUAL_STRING("Test Kunde A", a.name.c_str());
}

void test_tf04_entry_opens_and_closes_after_the_car_passed()
{
    SystemRig sys;
    TEST_ASSERT_EQUAL_STRING("LU123456 – Willkommen, Einfahrt frei", sys.entry(KZ_01).c_str());
    TEST_ASSERT_TRUE(BarrierState::Open == sys.state(GateLane::Entry)); // in the same request: no delay
    sys.wait(2);
    sys.hold(GateLane::Entry, true); // car on the loop
    sys.wait(10);
    TEST_ASSERT_TRUE(BarrierState::Open == sys.state(GateLane::Entry));
    sys.hold(GateLane::Entry, false); // car left the loop = passed
    sys.wait(1);
    TEST_ASSERT_TRUE(BarrierState::Closing == sys.state(GateLane::Entry));
    sys.wait(BARRIER_CLOSE_MS / 1000);
    TEST_ASSERT_TRUE(BarrierState::Closed == sys.state(GateLane::Entry));
    TEST_ASSERT_TRUE(BarrierState::Closed == sys.state(GateLane::Exit));
}

void test_tf05_terminal_feedback_at_entry()
{
    SystemRig sys;
    TEST_ASSERT_EQUAL_STRING("LU123456 – Willkommen, Einfahrt frei", sys.entry(KZ_01).c_str());
    TEST_ASSERT_TRUE(contains(sys.page("/kamera"), "LU123456 – Willkommen, Einfahrt frei"));
    sys.driveThrough(GateLane::Entry);
    sys.wait(10);
    TEST_ASSERT_EQUAL_STRING("BE4711 – nicht registriert, bitte Ruftaste drücken", sys.entry(KZ_03).c_str());
}

void test_tf06_normalized_plate_matches_at_exit()
{
    SystemRig sys;
    sys.entry(KZ_04);
    sys.driveThrough(GateLane::Entry);
    sys.wait(80 * MIN);
    TEST_ASSERT_TRUE(contains(sys.automatPay("LU123456", "card_ok"), "Bezahlt – CHF 4.00, Karte"));
    TEST_ASSERT_EQUAL_STRING("LU123456 – Gute Fahrt, Ausfahrt frei", sys.exit(" LU123456 ").c_str());
    TEST_ASSERT_EQUAL_UINT(1, sys.sessionCount("LU123456"));
    TEST_ASSERT_TRUE(SessionStatus::Closed == sys.sessions().findLatest("LU123456")->status);
}

void test_tf07_unregistered_stays_closed_no_session()
{
    SystemRig sys;
    TEST_ASSERT_EQUAL_STRING("BE4711 – nicht registriert, bitte Ruftaste drücken", sys.entry(KZ_03).c_str());
    sys.wait(10);
    TEST_ASSERT_TRUE(BarrierState::Closed == sys.state(GateLane::Entry));
    TEST_ASSERT_EQUAL_UINT(0, sys.sessions().all().size());
    TEST_ASSERT_TRUE(contains(sys.automat("BE4711"), "Kein offener Parkvorgang für BE4711"));
}

void test_tf08_ten_entry_cycles()
{
    SystemRig sys;
    for (int i = 0; i < 10; ++i)
    {
        const char* kz = i % 2 == 0 ? "LU123456" : "ZH98765";
        TEST_ASSERT_TRUE(contains(sys.entry(kz), "Willkommen, Einfahrt frei"));
        TEST_ASSERT_TRUE(BarrierState::Open == sys.state(GateLane::Entry));
        sys.driveThrough(GateLane::Entry);
        sys.wait(30 * MIN);
        TEST_ASSERT_TRUE(contains(sys.automatPay(kz, "twint"), "Bezahlt"));
        TEST_ASSERT_TRUE(contains(sys.exit(kz), "Gute Fahrt"));
        sys.driveThrough(GateLane::Exit);
        sys.wait(10);
    }
    TEST_ASSERT_EQUAL_UINT(10, sys.sessions().all().size());
}

void test_tf09_entry_time_and_running_duration()
{
    SystemRig sys;
    sys.entry(KZ_02);
    const ParkingSession* s = sys.sessions().findActive("ZH98765");
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT64(NOW, s->entry);
    std::string page = sys.automat("ZH98765");
    TEST_ASSERT_TRUE(contains(page, "<td>15.10.2025 14:06:40</td>"));
    TEST_ASSERT_TRUE(contains(page, "<td>0 h 0 min</td>"));
    sys.wait(60); // the duration is shown in whole minutes
    TEST_ASSERT_TRUE(contains(sys.automat("ZH98765"), "<td>0 h 1 min</td>"));
}

void test_tf10_second_entry_rejected()
{
    SystemRig sys;
    sys.entry(KZ_02);
    sys.driveThrough(GateLane::Entry);
    sys.wait(5 * MIN);
    TEST_ASSERT_EQUAL_STRING("Parkvorgang bereits offen – bitte Ruftaste drücken", sys.entry("ZH 98765").c_str());
    TEST_ASSERT_TRUE(BarrierState::Closed == sys.state(GateLane::Entry));
    TEST_ASSERT_EQUAL_UINT(1, sys.sessionCount("ZH98765"));
    TEST_ASSERT_EQUAL_INT64(NOW, sys.sessions().findActive("ZH98765")->entry);
}

void test_tf11_kasse_off_at_entry()
{
    SystemRig sys;
    sys.kasse.online = false;
    TEST_ASSERT_EQUAL_STRING("Kasse nicht erreichbar – bitte Ruftaste drücken", sys.entry(KZ_01).c_str());
    TEST_ASSERT_TRUE(BarrierState::Closed == sys.state(GateLane::Entry));
    TEST_ASSERT_EQUAL_UINT(0, sys.sessions().all().size());
    TEST_ASSERT_EQUAL_UINT(1, sys.eventCount(EventType::KasseUnreachable));
    sys.wait(2 * MIN);
    sys.kasse.online = true;
    TEST_ASSERT_TRUE(contains(sys.entry(KZ_01), "Willkommen"));
    TEST_ASSERT_EQUAL_INT64(NOW + 2 * MIN, sys.sessions().findActive("LU123456")->entry);
}

// Deviation from the Testkonzept (see README): Pico 1 answered after Pico 2's timeout, so the session exists
// although the barrier stayed closed; a retry within 30 s counts as the same entry.
void test_late_kasse_answer_then_retry_opens()
{
    SystemRig sys;
    TEST_ASSERT_EQUAL_INT(201, sys.kasse.postJson("/api/entry", "{\"kz\":\"LU123456\"}").status);
    sys.wait(3);
    TEST_ASSERT_EQUAL_STRING("LU123456 – Willkommen, Einfahrt frei", sys.entry(KZ_01).c_str());
    TEST_ASSERT_EQUAL_UINT(1, sys.sessionCount("LU123456"));
    TEST_ASSERT_EQUAL_INT64(NOW, sys.sessions().findActive("LU123456")->entry);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_tf01_register_survives_reboot);
    RUN_TEST(test_tf02_invalid_plates_not_stored);
    RUN_TEST(test_tf03_duplicate_plate_rejected);
    RUN_TEST(test_tf04_entry_opens_and_closes_after_the_car_passed);
    RUN_TEST(test_tf05_terminal_feedback_at_entry);
    RUN_TEST(test_tf06_normalized_plate_matches_at_exit);
    RUN_TEST(test_tf07_unregistered_stays_closed_no_session);
    RUN_TEST(test_tf08_ten_entry_cycles);
    RUN_TEST(test_tf09_entry_time_and_running_duration);
    RUN_TEST(test_tf10_second_entry_rejected);
    RUN_TEST(test_tf11_kasse_off_at_entry);
    RUN_TEST(test_late_kasse_answer_then_retry_opens);
    return UNITY_END();
}
