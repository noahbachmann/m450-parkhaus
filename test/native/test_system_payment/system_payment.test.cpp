#include <unity.h>

#include "system_rig.h"

// System test, Testkonzept 8.5 fee and payment, 8.6 exit and interface, 8.7 whole system (TF-18..TF-32).
// Payment at the Automat (public) and the Kasse (operator); the exit terminal pays by card or TWINT.

using namespace fixtures;

void setUp()
{
}

void tearDown()
{
}

void test_tf18_reference_fees_to_the_second()
{
    struct Case
    {
        const char* id;
        int64_t parked;
        const char* fee;
    };
    // The simulated clock does not move between entry and reading, so the exact boundaries of tariff.md work
    // end to end (on the hardware the Testkonzept uses values 1 min on the safe side).
    const Case cases[] = {
        {"R-01", 0, "CHF 0.00"},
        {"R-02", 15 * MIN, "CHF 0.00"},
        {"R-03", 15 * MIN + 1, "CHF 2.00"},
        {"R-04", HOUR, "CHF 2.00"},
        {"R-05", HOUR + 1, "CHF 4.00"},
        {"R-06", 2 * HOUR + 30 * MIN, "CHF 6.00"},
        {"R-07", 10 * HOUR, "CHF 20.00"},
        {"R-08", 10 * HOUR + 1, "CHF 20.00"},
        {"R-09", 24 * HOUR, "CHF 20.00"},
        {"R-10", 24 * HOUR + 1, "CHF 22.00"},
        {"R-11", 48 * HOUR, "CHF 40.00"},
    };
    for (const Case& c : cases)
    {
        SystemRig sys;
        std::string text = c.parked == 0 ? sys.entry(KZ_01) : sys.entryAgo(KZ_01, c.parked);
        TEST_ASSERT_TRUE_MESSAGE(contains(text, "Willkommen"), c.id);
        std::string page = sys.automat(KZ_01);
        TEST_ASSERT_TRUE_MESSAGE(contains(page, std::string("<th>Gebühr</th><td>") + c.fee + "</td>"), c.id);
    }

    SystemRig sys; // R-12: entry time 1 h in the future
    TEST_ASSERT_EQUAL_STRING("Einfahrtszeit liegt in der Zukunft", sys.entryAgo(KZ_01, -HOUR).c_str());
    TEST_ASSERT_EQUAL_UINT(0, sys.sessions().all().size());
    TEST_ASSERT_TRUE(BarrierState::Closed == sys.state(GateLane::Entry));
}

void test_tf19_duration_fee_and_tariff_at_kasse_and_automat()
{
    SystemRig sys;
    sys.entryAgo(KZ_01, HOUR + 20 * MIN);
    sys.login();
    for (const std::string& page : {sys.page("/kasse", {{"kz", KZ_01}}), sys.automat(KZ_01)})
    {
        TEST_ASSERT_TRUE(contains(page, "<td>LU123456</td>"));
        TEST_ASSERT_TRUE(contains(page, "<td>15.10.2025 12:46:40</td>"));
        TEST_ASSERT_TRUE(contains(page, "<td>1 h 20 min</td>"));
        TEST_ASSERT_TRUE(contains(page, "<td>CHF 4.00</td>"));
        TEST_ASSERT_TRUE(contains(page, "2 angefangene Stunden à CHF 2.00 ab Einfahrt (15 Freiminuten überschritten)"));
        TEST_ASSERT_TRUE(contains(page, "href=\"/tarif\""));
    }
    std::string tariff = sys.page("/tarif");
    TEST_ASSERT_TRUE(contains(tariff, "15 Freiminuten; wird die Freizeit überschritten, kostet jede angefangene "
                                      "Stunde ab Einfahrt CHF 2.00; Tagesmaximum CHF 20.00 pro angefangene 24 Stunden."));
}

void test_tf20_payment_at_kasse_once()
{
    SystemRig sys;
    sys.entryAgo(KZ_01, HOUR + 20 * MIN);
    sys.login();
    std::string paid = sys.submit("/kasse/pay", {{"kz", KZ_01}, {"method", "card_ok"}});
    TEST_ASSERT_TRUE(contains(paid, "Bezahlt – CHF 4.00, Karte – Transaktion TX-000001"));
    TEST_ASSERT_TRUE(contains(sys.kasse.getJson("/api/status?kz=LU123456").body, "\"status\":\"bezahlt\""));
    TEST_ASSERT_TRUE(contains(sys.submit("/kasse/pay", {{"kz", KZ_01}, {"method", "card_ok"}}), "bereits bezahlt"));
    TEST_ASSERT_EQUAL_UINT32(2, sys.sessions().nextTxNumber()); // no second booking
}

void test_tf21_automat_cash_change_and_twint()
{
    SystemRig sys;
    sys.entryAgo(KZ_01, 2 * HOUR + 30 * MIN);
    sys.entryAgo(KZ_02, 2 * HOUR + 30 * MIN);
    sys.entryAgo(KZ_08, 30 * MIN);

    TEST_ASSERT_TRUE(contains(sys.automatPay(KZ_01, "cash", "10.00"), "Rückgeld CHF 4.00"));
    TEST_ASSERT_TRUE(SessionStatus::Paid == sys.sessions().findActive("LU123456")->status);
    TEST_ASSERT_TRUE(contains(sys.automatPay(KZ_02, "twint"), "Bezahlt – CHF 6.00, TWINT"));
    TEST_ASSERT_TRUE(SessionStatus::Paid == sys.sessions().findActive("ZH98765")->status);

    TEST_ASSERT_TRUE(contains(sys.automatPay(KZ_08, "cash", "1.00"), "Restbetrag CHF 1.00 – unbezahlt"));
    TEST_ASSERT_TRUE(SessionStatus::Open == sys.sessions().findActive("AG1")->status);
    TEST_ASSERT_TRUE(contains(sys.automatPay(KZ_08, "cash", "1.00"), "Rückgeld CHF 0.00"));
    TEST_ASSERT_TRUE(SessionStatus::Paid == sys.sessions().findActive("AG1")->status);
}

void test_tf22_automat_without_session()
{
    SystemRig sys;
    std::string page = sys.automat(KZ_10);
    TEST_ASSERT_TRUE(contains(page, "Kein offener Parkvorgang für SZ555"));
    TEST_ASSERT_FALSE(contains(page, "/automat/pay"));
    TEST_ASSERT_TRUE(contains(sys.automatPay(KZ_10, "card_ok"), "Kein offener Parkvorgang für SZ555"));
    TEST_ASSERT_EQUAL_UINT32(1, sys.sessions().nextTxNumber());
}

void test_tf23_paid_exit_closes_session()
{
    SystemRig sys;
    sys.entryAgo(KZ_01, HOUR + 20 * MIN);
    sys.automatPay(KZ_01, "card_ok");
    sys.wait(MIN);
    int64_t exitTime = sys.now;
    TEST_ASSERT_EQUAL_STRING("LU123456 – Gute Fahrt, Ausfahrt frei", sys.exit(KZ_01).c_str());
    TEST_ASSERT_TRUE(BarrierState::Open == sys.state(GateLane::Exit));
    const ParkingSession* s = sys.sessions().findLatest("LU123456");
    TEST_ASSERT_TRUE(SessionStatus::Closed == s->status);
    TEST_ASSERT_EQUAL_INT64(exitTime, s->exit);
    sys.login();
    TEST_ASSERT_TRUE(contains(sys.page("/kasse", {{"kz", KZ_01}}), "<td>abgeschlossen</td>"));
    sys.wait(MIN);
    TEST_ASSERT_TRUE(contains(sys.page("/kasse", {{"kz", KZ_01}}), "<td>1 h 21 min</td>")); // clock stopped
}

void test_tf24_unpaid_exit_then_pay_then_exit()
{
    SystemRig sys;
    sys.entryAgo(KZ_02, 2 * HOUR + 30 * MIN);
    TEST_ASSERT_EQUAL_STRING("Gebühr CHF 6.00 nicht bezahlt – bitte am Automaten bezahlen", sys.exit(KZ_02).c_str());
    sys.wait(10);
    TEST_ASSERT_TRUE(BarrierState::Closed == sys.state(GateLane::Exit));
    TEST_ASSERT_TRUE(SessionStatus::Open == sys.sessions().findActive("ZH98765")->status);
    sys.automatPay(KZ_02, "twint");
    TEST_ASSERT_TRUE(contains(sys.exit(KZ_02), "Gute Fahrt"));
}

void test_tf25_free_time_exit()
{
    SystemRig sys;
    sys.entryAgo(KZ_08, 5 * MIN);
    TEST_ASSERT_TRUE(contains(sys.exit(KZ_08), "Gute Fahrt"));
    const ParkingSession* s = sys.sessions().findLatest("AG1");
    TEST_ASSERT_TRUE(SessionStatus::ClosedFree == s->status);
    TEST_ASSERT_EQUAL_UINT32(0, s->amountRappen);
    sys.login();
    std::string page = sys.page("/kasse", {{"kz", KZ_08}});
    TEST_ASSERT_TRUE(contains(page, "<td>CHF 0.00</td>"));
    TEST_ASSERT_TRUE(contains(page, "abgeschlossen (Freizeit)"));
}

void test_tf26_exit_without_session()
{
    SystemRig sys;
    TEST_ASSERT_EQUAL_STRING("Kein Parkvorgang gefunden – bitte Ruftaste drücken", sys.exit(KZ_10).c_str());
    TEST_ASSERT_TRUE(BarrierState::Closed == sys.state(GateLane::Exit));
}

void test_tf27_car_turns_around()
{
    SystemRig sys;
    sys.entryAgo(KZ_02, 58 * MIN);
    TEST_ASSERT_EQUAL_STRING("Gebühr CHF 2.00 nicht bezahlt – bitte am Automaten bezahlen", sys.exit(KZ_02).c_str());
    TEST_ASSERT_EQUAL_STRING("Vorgang abgebrochen – Fahrzeug wendet", sys.turnAround().c_str());
    const ParkingSession* s = sys.sessions().findActive("ZH98765");
    TEST_ASSERT_TRUE(SessionStatus::Open == s->status);
    TEST_ASSERT_EQUAL_INT64(0, s->exit);
    TEST_ASSERT_TRUE(contains(sys.automat(KZ_02), "<td>CHF 2.00</td>"));
    sys.wait(3 * MIN);
    std::string page = sys.automat(KZ_02);
    TEST_ASSERT_TRUE(contains(page, "<td>CHF 4.00</td>"));
    TEST_ASSERT_TRUE(contains(page, "<td>offen</td>"));
}

void test_tf28_declined_payment_at_exit_terminal()
{
    SystemRig sys;
    sys.entryAgo(KZ_01, HOUR + 20 * MIN);
    sys.entryAgo(KZ_02, 2 * HOUR + 30 * MIN);

    sys.exit(KZ_01);
    TEST_ASSERT_EQUAL_STRING("Zahlung abgelehnt – anderes Zahlungsmittel wählen", sys.payAtExit("card_declined").c_str());
    TEST_ASSERT_TRUE(BarrierState::Closed == sys.state(GateLane::Exit));
    TEST_ASSERT_TRUE(contains(sys.payAtExit("twint"), "Gute Fahrt"));
    TEST_ASSERT_TRUE(BarrierState::Open == sys.state(GateLane::Exit));
    sys.driveThrough(GateLane::Exit);
    sys.wait(10);

    sys.exit(KZ_02);
    for (int i = 0; i < 4; ++i)
    {
        TEST_ASSERT_EQUAL_STRING("Zahlung abgelehnt – anderes Zahlungsmittel wählen",
                                 sys.payAtExit("card_declined").c_str());
    }
    TEST_ASSERT_EQUAL_STRING("Zahlung abgelehnt – Leitstelle wurde benachrichtigt",
                             sys.payAtExit("card_declined").c_str());
    TEST_ASSERT_TRUE(BarrierState::Closed == sys.state(GateLane::Exit));
    TEST_ASSERT_EQUAL_UINT(1, sys.eventCount(EventType::PaymentAlert));
    sys.login();
    std::string page = sys.page("/betrieb");
    TEST_ASSERT_TRUE(contains(page, "Zahlung 5× abgelehnt – ZH98765"));
    TEST_ASSERT_TRUE(contains(page, "<td>Zahlung 5× abgelehnt</td><td>Ausfahrt</td><td>ZH98765</td>"));
}

void test_tf29_status_interface_for_all_states()
{
    SystemRig sys;
    sys.entryAgo(KZ_01, HOUR + 20 * MIN);
    sys.automatPay(KZ_01, "card_ok");
    sys.entryAgo(KZ_02, 2 * HOUR + 30 * MIN);
    struct Case
    {
        const char* query;
        int status;
        const char* part;
    };
    const Case cases[] = {
        {"LU123456", 200, "\"status\":\"bezahlt\""},
        {"ZH98765", 200, "\"status\":\"unbezahlt\""},
        {"SZ555", 200, "\"status\":\"unbekannt\""},
        {"", 400, "invalid_plate"},
        {"LU-12%2345", 400, "invalid_plate"}, // «LU-12#45», URL-encoded like curl --data-urlencode
    };
    for (int round = 0; round < 4; ++round) // 20 requests, the answers must not change
    {
        for (const Case& c : cases)
        {
            HttpReply r = sys.kasse.getJson(std::string("/api/status?kz=") + c.query);
            TEST_ASSERT_EQUAL_INT_MESSAGE(c.status, r.status, c.query);
            TEST_ASSERT_TRUE_MESSAGE(contains(r.body, c.part), r.body.c_str());
        }
    }
}

void test_tf30_kasse_off_at_exit_logged_and_paid_status_kept()
{
    SystemRig sys;
    sys.entryAgo(KZ_01, HOUR + 20 * MIN);
    sys.automatPay(KZ_01, "card_ok");
    sys.entryAgo(KZ_02, 2 * HOUR + 30 * MIN);
    sys.kasse.online = false;
    TEST_ASSERT_EQUAL_STRING("Kasse nicht erreichbar – bitte Ruftaste drücken", sys.exit(KZ_02).c_str());
    TEST_ASSERT_EQUAL_STRING("Kasse nicht erreichbar – bitte Ruftaste drücken", sys.exit(KZ_01).c_str());
    TEST_ASSERT_TRUE(BarrierState::Closed == sys.state(GateLane::Exit));
    TEST_ASSERT_EQUAL_UINT(2, sys.eventCount(EventType::KasseUnreachable));
    const Event& logged = sys.app().events().all().back();
    TEST_ASSERT_TRUE(GateLane::Exit == logged.lane);
    TEST_ASSERT_EQUAL_STRING("LU123456", logged.plate.c_str());

    sys.rebootKasse();
    sys.kasse.online = true;
    TEST_ASSERT_TRUE(SessionStatus::Paid == sys.sessions().findActive("LU123456")->status);
    TEST_ASSERT_TRUE(contains(sys.exit(KZ_01), "Gute Fahrt"));
}

void test_tf31_remote_opening_at_exit_without_payment()
{
    SystemRig sys;
    sys.entryAgo(KZ_02, 2 * HOUR + 30 * MIN);
    sys.exit(KZ_02);
    sys.login();
    TEST_ASSERT_TRUE(contains(sys.page("/betrieb"), "unbezahlt (CHF 6.00)"));
    sys.remoteOpen(GateLane::Exit, "ZH98765", "Kulanz");
    TEST_ASSERT_TRUE(BarrierState::Open == sys.state(GateLane::Exit));
    const Event& e = sys.app().events().all().back();
    TEST_ASSERT_EQUAL_STRING("betrieb", e.user.c_str());
    TEST_ASSERT_TRUE(GateLane::Exit == e.lane);
    TEST_ASSERT_EQUAL_STRING("ZH98765", e.plate.c_str());
    TEST_ASSERT_EQUAL_STRING("Kulanz", e.reason.c_str());
    TEST_ASSERT_EQUAL_STRING("ohne Zahlung", e.note.c_str());
    TEST_ASSERT_EQUAL_INT64(sys.now, e.time);
    TEST_ASSERT_TRUE(contains(sys.page("/kasse", {{"kz", KZ_02}}), "<td>ohne Zahlung ausgefahren</td>"));
}

void test_tf32_whole_system()
{
    SystemRig sys;
    sys.login();
    TEST_ASSERT_TRUE(contains(sys.submit("/registrierung/add", {{"name", "Test Kunde F"}, {"kz", KZ_11}}),
                              "Kennzeichen registriert"));
    sys.cookie.clear(); // the customer is not logged in
    TEST_ASSERT_TRUE(contains(sys.entryAgo(KZ_11, 2 * HOUR + 30 * MIN), "Willkommen"));
    sys.driveThrough(GateLane::Entry);
    TEST_ASSERT_TRUE(contains(sys.automat(KZ_11), "<td>CHF 6.00</td>"));
    TEST_ASSERT_EQUAL_STRING("Gebühr CHF 6.00 nicht bezahlt – bitte am Automaten bezahlen", sys.exit(KZ_11).c_str());
    TEST_ASSERT_TRUE(contains(sys.automatPay(KZ_11, "cash", "10.00"), "Rückgeld CHF 4.00"));
    TEST_ASSERT_TRUE(contains(sys.exit(KZ_11), "Gute Fahrt"));
    TEST_ASSERT_TRUE(SessionStatus::Closed == sys.sessions().findLatest("OW2026")->status);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_tf18_reference_fees_to_the_second);
    RUN_TEST(test_tf19_duration_fee_and_tariff_at_kasse_and_automat);
    RUN_TEST(test_tf20_payment_at_kasse_once);
    RUN_TEST(test_tf21_automat_cash_change_and_twint);
    RUN_TEST(test_tf22_automat_without_session);
    RUN_TEST(test_tf23_paid_exit_closes_session);
    RUN_TEST(test_tf24_unpaid_exit_then_pay_then_exit);
    RUN_TEST(test_tf25_free_time_exit);
    RUN_TEST(test_tf26_exit_without_session);
    RUN_TEST(test_tf27_car_turns_around);
    RUN_TEST(test_tf28_declined_payment_at_exit_terminal);
    RUN_TEST(test_tf29_status_interface_for_all_states);
    RUN_TEST(test_tf30_kasse_off_at_exit_logged_and_paid_status_kept);
    RUN_TEST(test_tf31_remote_opening_at_exit_without_payment);
    RUN_TEST(test_tf32_whole_system);
    return UNITY_END();
}
