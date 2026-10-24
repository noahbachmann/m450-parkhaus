#include <unity.h>

#include "kasse_api.h"
#include "session.h"
#include "terminal.h"

namespace
{
    constexpr int64_t NOW = 1760530000; // 15.10.2025 14:06:40 Zurich (CEST)
    constexpr int64_t MIN = 60;

    // Pico 1's real API handlers on a real store.
    class LocalKasse : public KasseLink
    {
    public:
        SessionStore store;
        int64_t now = NOW;
        bool online = true;

        HttpReply postJson(const char* path, const std::string& body) override
        {
            if (!online)
            {
                return {};
            }
            std::string p = path;
            ApiResponse r = p == "/api/exit"  ? handleExit(store, body, now, true)
                            : p == "/api/pay" ? handlePay(store, body, now, true)
                                              : handleEntry(store, body, now, true);
            return {r.status, r.body};
        }

        HttpReply getJson(const std::string&) override
        {
            return {};
        }
    };

    LaneTerminal exitTerminal()
    {
        LaneTerminal t;
        t.lane = GateLane::Exit;
        return t;
    }
}

void setUp()
{
}

void tearDown()
{
}

void test_unpaid_exit_offers_payment()
{
    LocalKasse kasse;
    kasse.store.openEntry("ZH98765", NOW - 150 * MIN);
    LaneTerminal t = exitTerminal();
    recordGate(t, exitRequest("ZH98765", kasse));
    TEST_ASSERT_EQUAL_STRING("ZH98765", t.payPlate.c_str());
    TEST_ASSERT_EQUAL_UINT32(600, t.payFeeRappen);
    recordGate(t, exitRequest("SZ555", kasse)); // other result: offer gone
    TEST_ASSERT_EQUAL_STRING("", t.payPlate.c_str());
}

void test_tf28_declined_then_twint_opens()
{
    LocalKasse kasse;
    kasse.store.openEntry("LU123456", NOW - 80 * MIN);
    EventLog log;
    LaneTerminal t = exitTerminal();
    recordGate(t, exitRequest("LU123456", kasse));

    GateResult declined = payAtTerminal(t, "card_declined", NOW, kasse, log);
    TEST_ASSERT_FALSE(declined.open);
    TEST_ASSERT_EQUAL_STRING("Zahlung abgelehnt – anderes Zahlungsmittel wählen", declined.message.c_str());
    TEST_ASSERT_EQUAL(SessionStatus::Open, kasse.store.findActive("LU123456")->status);

    GateResult twint = payAtTerminal(t, "twint", NOW, kasse, log);
    TEST_ASSERT_TRUE(twint.open);
    TEST_ASSERT_EQUAL_STRING("Bezahlt – CHF 4.00, TWINT – LU123456 – Gute Fahrt, Ausfahrt frei", twint.message.c_str());
    TEST_ASSERT_EQUAL(SessionStatus::Closed, kasse.store.findLatest("LU123456")->status);
    TEST_ASSERT_EQUAL_STRING("", t.payPlate.c_str());
    TEST_ASSERT_EQUAL_UINT(0, log.all().size());
}

void test_tf28_five_declines_notify_operator()
{
    LocalKasse kasse;
    kasse.store.openEntry("ZH98765", NOW - 150 * MIN);
    EventLog log;
    LaneTerminal t = exitTerminal();
    recordGate(t, exitRequest("ZH98765", kasse));
    for (int i = 1; i <= 4; ++i)
    {
        GateResult r = payAtTerminal(t, "card_declined", NOW + i, kasse, log);
        TEST_ASSERT_EQUAL_STRING("Zahlung abgelehnt – anderes Zahlungsmittel wählen", r.message.c_str());
        TEST_ASSERT_FALSE(t.callPending);
    }
    GateResult fifth = payAtTerminal(t, "card_declined", NOW + 5, kasse, log);
    TEST_ASSERT_FALSE(fifth.open);
    TEST_ASSERT_EQUAL_STRING("Zahlung abgelehnt – Leitstelle wurde benachrichtigt", fifth.message.c_str());
    TEST_ASSERT_TRUE(t.callPending);
    TEST_ASSERT_EQUAL_STRING("Zahlung 5× abgelehnt – ZH98765 (15.10.2025 14:06:45)", callLabel(t).c_str());
    TEST_ASSERT_EQUAL_UINT(1, log.all().size());
    TEST_ASSERT_EQUAL(EventType::PaymentAlert, log.all()[0].type);
    TEST_ASSERT_EQUAL(GateLane::Exit, log.all()[0].lane);
    TEST_ASSERT_EQUAL_STRING("ZH98765", log.all()[0].plate.c_str());

    payAtTerminal(t, "card_declined", NOW + 6, kasse, log); // no second alert
    TEST_ASSERT_EQUAL_UINT(1, log.all().size());
    TEST_ASSERT_EQUAL(SessionStatus::Open, kasse.store.findActive("ZH98765")->status);
}

void test_declines_reset_for_another_plate()
{
    LocalKasse kasse;
    kasse.store.openEntry("ZH98765", NOW - 150 * MIN);
    kasse.store.openEntry("LU123456", NOW - 150 * MIN);
    EventLog log;
    LaneTerminal t = exitTerminal();
    recordGate(t, exitRequest("ZH98765", kasse));
    payAtTerminal(t, "card_declined", NOW, kasse, log);
    recordGate(t, exitRequest("ZH98765", kasse));
    TEST_ASSERT_EQUAL_INT(1, t.declines);
    recordGate(t, exitRequest("LU123456", kasse));
    TEST_ASSERT_EQUAL_INT(0, t.declines);
}

void test_tf27_abort_keeps_session_open()
{
    LocalKasse kasse;
    kasse.store.openEntry("ZH98765", NOW - 58 * MIN);
    LaneTerminal t = exitTerminal();
    GateResult unpaid = exitRequest("ZH98765", kasse);
    TEST_ASSERT_EQUAL_STRING("Gebühr CHF 2.00 nicht bezahlt – bitte hier oder an der Kasse bezahlen", unpaid.message.c_str());
    recordGate(t, unpaid);
    GateResult r = abortAtTerminal(t);
    TEST_ASSERT_FALSE(r.open);
    TEST_ASSERT_EQUAL_STRING("Vorgang abgebrochen – Fahrzeug wendet", r.message.c_str());
    TEST_ASSERT_EQUAL_STRING("", t.payPlate.c_str());
    const ParkingSession* s = kasse.store.findActive("ZH98765");
    TEST_ASSERT_EQUAL(SessionStatus::Open, s->status);
    TEST_ASSERT_EQUAL_INT64(0, s->exit);
    uint32_t fee = 0;
    TEST_ASSERT_TRUE(currentFee(*s, NOW + 3 * MIN, fee));
    TEST_ASSERT_EQUAL_UINT32(400, fee);
}

void test_call_button()
{
    EventLog log;
    LaneTerminal t;
    t.lane = GateLane::Entry;
    GateResult unregistered;
    unregistered.plate = "BE4711";
    recordGate(t, unregistered);
    GateResult r = pressCallButton(t, NOW, log);
    TEST_ASSERT_EQUAL_STRING("Leitstelle wurde gerufen – bitte warten", r.message.c_str());
    TEST_ASSERT_EQUAL_STRING("Ruftaste gedrückt – BE4711 (15.10.2025 14:06:40)", callLabel(t).c_str());
    TEST_ASSERT_EQUAL(EventType::CallButton, log.all()[0].type);
    TEST_ASSERT_EQUAL(GateLane::Entry, log.all()[0].lane);
    TEST_ASSERT_EQUAL_STRING("BE4711", log.all()[0].plate.c_str());
    recordRemoteOpening(t, "BE4711");
    TEST_ASSERT_EQUAL_STRING("", callLabel(t).c_str());
    TEST_ASSERT_EQUAL_STRING("BE4711", t.lastPlate.c_str());
}

void test_pay_without_offer_or_kasse()
{
    LocalKasse kasse;
    EventLog log;
    LaneTerminal t = exitTerminal();
    TEST_ASSERT_EQUAL_STRING("Kein Zahlvorgang – bitte Kennzeichen an der Ausfahrt erfassen",
                             payAtTerminal(t, "twint", NOW, kasse, log).message.c_str());
    kasse.store.openEntry("ZH98765", NOW - 150 * MIN);
    recordGate(t, exitRequest("ZH98765", kasse));
    TEST_ASSERT_EQUAL_STRING("Bitte Zahlungsmittel wählen", payAtTerminal(t, "cash", NOW, kasse, log).message.c_str());
    kasse.online = false;
    GateResult r = payAtTerminal(t, "twint", NOW, kasse, log);
    TEST_ASSERT_FALSE(r.open);
    TEST_ASSERT_TRUE(r.unreachable);
    TEST_ASSERT_EQUAL_STRING("ZH98765", t.payPlate.c_str());
}

void test_paid_elsewhere_meanwhile_just_exits()
{
    LocalKasse kasse;
    kasse.store.openEntry("ZH98765", NOW - 150 * MIN);
    EventLog log;
    LaneTerminal t = exitTerminal();
    recordGate(t, exitRequest("ZH98765", kasse));
    handlePay(kasse.store, "{\"kz\":\"ZH98765\",\"method\":\"twint\"}", NOW, true); // paid at the Kasse
    GateResult r = payAtTerminal(t, "card_ok", NOW, kasse, log);
    TEST_ASSERT_TRUE(r.open);
    TEST_ASSERT_EQUAL_STRING("ZH98765 – Gute Fahrt, Ausfahrt frei", r.message.c_str());
    TEST_ASSERT_EQUAL_UINT32(2, kasse.store.nextTxNumber()); // no double booking
}

void test_drive_through_then_closed_message()
{
    LaneTerminal t;
    t.lane = GateLane::Entry;
    GateResult opened;
    opened.open = true;
    opened.plate = "LU123456";
    recordGate(t, opened);
    TEST_ASSERT_EQUAL_STRING("", closedAfterPass(t).message.c_str()); // nothing passed yet
    GateResult r = driveThrough(t, true);
    TEST_ASSERT_EQUAL_STRING("ok", r.cls);
    TEST_ASSERT_EQUAL_STRING("LU123456 durchgefahren – Schranke schliesst", r.message.c_str());
    TEST_ASSERT_TRUE(t.passing);
    GateResult closed = closedAfterPass(t);
    TEST_ASSERT_EQUAL_STRING("ok", closed.cls);
    TEST_ASSERT_EQUAL_STRING("LU123456 durchgefahren – Schranke geschlossen", closed.message.c_str());
    TEST_ASSERT_EQUAL_STRING("", closedAfterPass(t).message.c_str()); // once
}

void test_drive_through_without_open_barrier()
{
    LaneTerminal t = exitTerminal();
    GateResult r = driveThrough(t, false);
    TEST_ASSERT_EQUAL_STRING("err", r.cls);
    TEST_ASSERT_EQUAL_STRING("Keine offene Schranke – Fahrzeug kann nicht durchfahren", r.message.c_str());
    TEST_ASSERT_FALSE(t.passing);
    TEST_ASSERT_EQUAL_STRING("Fahrzeug durchgefahren – Schranke schliesst", driveThrough(t, true).message.c_str());
}

void test_new_opening_clears_pass()
{
    LaneTerminal t = exitTerminal();
    driveThrough(t, true);
    recordRemoteOpening(t, "ZH98765");
    TEST_ASSERT_FALSE(t.passing);
    driveThrough(t, true);
    GateResult opened;
    opened.open = true;
    opened.plate = "LU123456";
    recordGate(t, opened);
    TEST_ASSERT_FALSE(t.passing);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_unpaid_exit_offers_payment);
    RUN_TEST(test_tf28_declined_then_twint_opens);
    RUN_TEST(test_tf28_five_declines_notify_operator);
    RUN_TEST(test_declines_reset_for_another_plate);
    RUN_TEST(test_tf27_abort_keeps_session_open);
    RUN_TEST(test_call_button);
    RUN_TEST(test_pay_without_offer_or_kasse);
    RUN_TEST(test_paid_elsewhere_meanwhile_just_exits);
    RUN_TEST(test_drive_through_then_closed_message);
    RUN_TEST(test_drive_through_without_open_barrier);
    RUN_TEST(test_new_opening_clears_pass);
    return UNITY_END();
}
