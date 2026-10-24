#include <unity.h>

#include <string>

#include "kasse_api.h"
#include "kasse_client.h"
#include "payment.h"
#include "session.h"

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
        bool synced = true;

        HttpReply postJson(const char* path, const std::string& body) override
        {
            if (!online)
            {
                return {};
            }
            std::string p = path;
            ApiResponse r = p == "/api/pay"     ? handlePay(store, body, now, synced)
                            : p == "/api/reset" ? handleReset(store)
                                                : ApiResponse{404, "{\"error\":\"not_found\"}"};
            return {r.status, r.body};
        }

        HttpReply getJson(const std::string& pathAndQuery) override
        {
            if (!online)
            {
                return {};
            }
            ApiResponse r = pathAndQuery == "/api/sessions" ? handleSessions(store, now, synced)
                                                            : ApiResponse{404, "{\"error\":\"not_found\"}"};
            return {r.status, r.body};
        }
    };

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
}

void setUp()
{
}

void tearDown()
{
}

void test_fetch_sessions()
{
    LocalKasse kasse;
    kasse.store.openEntry("LU123456", NOW - 80 * MIN);
    pay(kasse.store, "LU123456", PayMethod::Twint, NOW);
    kasse.store.openEntry("ZH98765", NOW - 5 * MIN);
    KasseSnapshot snap = fetchSessions(kasse);
    TEST_ASSERT_TRUE(snap.reachable());
    TEST_ASSERT_TRUE(snap.synced);
    TEST_ASSERT_EQUAL_INT64(NOW, snap.now);
    TEST_ASSERT_EQUAL_UINT(2, snap.sessions.all().size());
    TEST_ASSERT_TRUE(SessionStatus::Paid == snap.sessions.findActive("LU123456")->status);
    TEST_ASSERT_EQUAL_STRING("TX-000001", snap.sessions.findActive("LU123456")->txRef.c_str());

    kasse.synced = false;
    TEST_ASSERT_FALSE(fetchSessions(kasse).synced);
}

void test_fetch_sessions_unreachable_or_garbage()
{
    LocalKasse kasse;
    kasse.online = false;
    TEST_ASSERT_FALSE(fetchSessions(kasse).reachable());
    FixedReply garbage;
    garbage.reply = {200, "{\"sessions\":\"nope\"}"};
    TEST_ASSERT_FALSE(fetchSessions(garbage).reachable());
    garbage.reply = {500, "{\"nextTx\":1,\"sessions\":[],\"now\":1}"};
    TEST_ASSERT_FALSE(fetchSessions(garbage).reachable());
    garbage.reply = {200, "{\"nextTx\":1,\"sessions\":[],\"now\":1,\"synced\":\"yes\"}"};
    TEST_ASSERT_FALSE(fetchSessions(garbage).reachable());
    TEST_ASSERT_EQUAL_STRING("Kasse nicht erreichbar", fetchSessions(kasse).error.c_str());
    TEST_ASSERT_EQUAL_STRING("Unerwartete Antwort der Kasse", fetchSessions(garbage).error.c_str());
}

void test_pay_tf20_card_then_already_paid()
{
    LocalKasse kasse;
    kasse.store.openEntry("LU123456", NOW - 80 * MIN);
    FormResult first = payAtKasse(kasse, "lu 123456", "card_ok", "");
    TEST_ASSERT_TRUE(first.changed);
    TEST_ASSERT_EQUAL_STRING("ok", first.notice.cls);
    TEST_ASSERT_EQUAL_STRING("Bezahlt – CHF 4.00, Karte – Transaktion TX-000001", first.notice.text.c_str());

    kasse.now = NOW + 5; // double click / second browser
    FormResult second = payAtKasse(kasse, "LU123456", "card_ok", "");
    TEST_ASSERT_FALSE(second.changed);
    TEST_ASSERT_EQUAL_STRING("err", second.notice.cls);
    TEST_ASSERT_EQUAL_STRING("bereits bezahlt", second.notice.text.c_str());
    TEST_ASSERT_EQUAL_UINT32(2, kasse.store.nextTxNumber());
}

void test_pay_tf21_cash()
{
    LocalKasse kasse;
    kasse.store.openEntry("LU123456", NOW - 150 * MIN);
    FormResult change = payAtKasse(kasse, "LU123456", "cash", "10.00");
    TEST_ASSERT_EQUAL_STRING("Bezahlt – CHF 6.00, Bargeld – Transaktion TX-000001 – Rückgeld CHF 4.00",
                             change.notice.text.c_str());

    kasse.store.openEntry("AG1", NOW - 30 * MIN);
    FormResult partial = payAtKasse(kasse, "AG1", "cash", "1");
    TEST_ASSERT_TRUE(partial.changed);
    TEST_ASSERT_EQUAL_STRING("info", partial.notice.cls);
    TEST_ASSERT_EQUAL_STRING("Restbetrag CHF 1.00 – unbezahlt", partial.notice.text.c_str());
    FormResult rest = payAtKasse(kasse, "AG1", "cash", "1,00");
    TEST_ASSERT_EQUAL_STRING("ok", rest.notice.cls);
    TEST_ASSERT_EQUAL_STRING("Bezahlt – CHF 2.00, Bargeld – Transaktion TX-000002 – Rückgeld CHF 0.00",
                             rest.notice.text.c_str());
}

void test_pay_rejects_bad_input()
{
    LocalKasse kasse;
    kasse.store.openEntry("LU123456", NOW - 80 * MIN);
    TEST_ASSERT_EQUAL_STRING("Bitte Zahlungsmittel wählen",
                             payAtKasse(kasse, "LU123456", "bitcoin", "").notice.text.c_str());
    for (const char* amount : {"zehn", "", "0", "-5", "1000.01"})
    {
        TEST_ASSERT_EQUAL_STRING("Bitte eingeworfenen Betrag in CHF eingeben",
                                 payAtKasse(kasse, "LU123456", "cash", amount).notice.text.c_str());
    }
    FormResult declined = payAtKasse(kasse, "LU123456", "card_declined", "");
    TEST_ASSERT_FALSE(declined.changed);
    TEST_ASSERT_EQUAL_STRING("Zahlung abgelehnt – anderes Zahlungsmittel wählen", declined.notice.text.c_str());
    TEST_ASSERT_EQUAL_STRING("Kein offener Parkvorgang für SZ555",
                             payAtKasse(kasse, "SZ 555", "twint", "").notice.text.c_str());
    TEST_ASSERT_EQUAL_STRING("Ungültiges Kennzeichen", payAtKasse(kasse, "L1", "twint", "").notice.text.c_str());
    kasse.synced = false;
    TEST_ASSERT_EQUAL_STRING("Uhrzeit nicht synchronisiert – bitte später erneut versuchen",
                             payAtKasse(kasse, "LU123456", "card_ok", "").notice.text.c_str());
    TEST_ASSERT_TRUE(SessionStatus::Open == kasse.store.findActive("LU123456")->status);
}

void test_pay_kasse_unreachable_or_unexpected()
{
    LocalKasse kasse;
    kasse.online = false;
    FormResult down = payAtKasse(kasse, "LU123456", "card_ok", "");
    TEST_ASSERT_EQUAL_STRING("err", down.notice.cls);
    TEST_ASSERT_EQUAL_STRING("Kasse nicht erreichbar – bitte später erneut versuchen", down.notice.text.c_str());
    FixedReply odd;
    odd.reply = {200, "{\"kz\":\"ZH98765\",\"status\":\"bezahlt\",\"amount\":600,\"method\":\"card\",\"tx\":\"X\"}"};
    FormResult wrongPlate = payAtKasse(odd, "LU123456", "card_ok", "");
    TEST_ASSERT_FALSE(wrongPlate.changed);
    TEST_ASSERT_EQUAL_STRING("Unerwartete Antwort der Kasse (200)", wrongPlate.notice.text.c_str());
    odd.reply = {200, "{\"kz\":\"LU123456\",\"status\":\"bezahlt\",\"amount\":600,\"method\":\"gold\",\"tx\":\"X\"}"};
    TEST_ASSERT_EQUAL_STRING("Unerwartete Antwort der Kasse (200)",
                             payAtKasse(odd, "LU123456", "card_ok", "").notice.text.c_str());
}

void test_reset()
{
    LocalKasse kasse;
    kasse.store.openEntry("LU123456", NOW - 80 * MIN);
    TEST_ASSERT_TRUE(resetKasse(kasse));
    TEST_ASSERT_EQUAL_UINT(0, kasse.store.all().size());
    kasse.online = false;
    TEST_ASSERT_FALSE(resetKasse(kasse));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_fetch_sessions);
    RUN_TEST(test_fetch_sessions_unreachable_or_garbage);
    RUN_TEST(test_pay_tf20_card_then_already_paid);
    RUN_TEST(test_pay_tf21_cash);
    RUN_TEST(test_pay_rejects_bad_input);
    RUN_TEST(test_pay_kasse_unreachable_or_unexpected);
    RUN_TEST(test_reset);
    return UNITY_END();
}
