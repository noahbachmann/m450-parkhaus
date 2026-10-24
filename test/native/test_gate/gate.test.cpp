#include <unity.h>

#include "gate.h"
#include "kasse_api.h"
#include "registration.h"
#include "session.h"
#include "test_fixtures.h"
#include "testdata.h"

using namespace fixtures;

namespace
{
    class FakeKasse : public KasseLink
    {
    public:
        HttpReply reply;
        std::string lastPath;
        std::string lastBody;
        int calls = 0;

        HttpReply postJson(const char* path, const std::string& body) override
        {
            calls++;
            lastPath = path;
            lastBody = body;
            return reply;
        }

        HttpReply getJson(const std::string& pathAndQuery) override
        {
            calls++;
            lastPath = pathAndQuery;
            return reply;
        }
    };

    RegistrationList regs;
    FakeKasse fake;
}

void setUp()
{
    loadDefaultRegistrations(regs);
    fake = FakeKasse();
}

void tearDown()
{
}

void test_tf04_tf05_entry_registered_opens()
{
    fake.reply = {201, "{\"kz\":\"LU123456\",\"entry\":1760530000}"};
    GateResult r = entryRequest(regs, "lu 123 456", "", fake);
    TEST_ASSERT_TRUE(r.open);
    TEST_ASSERT_EQUAL_STRING("ok", r.cls);
    TEST_ASSERT_EQUAL_STRING("LU123456 – Willkommen, Einfahrt frei", r.message.c_str());
    TEST_ASSERT_EQUAL_STRING("/api/entry", fake.lastPath.c_str());
    TEST_ASSERT_EQUAL_STRING("{\"kz\":\"LU123456\"}", fake.lastBody.c_str());
}

void test_tf07_entry_unregistered_stays_closed_without_request()
{
    fake.reply = {201, "{\"kz\":\"BE4711\",\"entry\":1760530000}"};
    GateResult r = entryRequest(regs, "BE 4711", "", fake);
    TEST_ASSERT_FALSE(r.open);
    TEST_ASSERT_EQUAL_STRING("BE4711 – nicht registriert, bitte Ruftaste drücken", r.message.c_str());
    TEST_ASSERT_EQUAL_INT(0, fake.calls);
}

void test_entry_invalid_plate_no_request()
{
    TEST_ASSERT_EQUAL_STRING("Bitte Kennzeichen eingeben", entryRequest(regs, "  ", "", fake).message.c_str());
    GateResult r = entryRequest(regs, "LU-12#45", "", fake);
    TEST_ASSERT_FALSE(r.open);
    TEST_ASSERT_EQUAL_STRING("Ungültiges Kennzeichen – erwartet 2 Buchstaben und 1–6 Ziffern", r.message.c_str());
    TEST_ASSERT_EQUAL_INT(0, fake.calls);
}

void test_entry_backdated_sends_epoch()
{
    fake.reply = {201, "{\"kz\":\"LU123456\",\"entry\":1760521000}"};
    GateResult r = entryRequest(regs, "LU123456", "2025-10-15T11:36:40", fake);
    TEST_ASSERT_TRUE(r.open);
    TEST_ASSERT_EQUAL_STRING("{\"kz\":\"LU123456\",\"entry\":1760521000}", fake.lastBody.c_str());

    GateResult bad = entryRequest(regs, "LU123456", "gestern", fake);
    TEST_ASSERT_FALSE(bad.open);
    TEST_ASSERT_EQUAL_STRING("Ungültige Einfahrtszeit – Format TT.MM.JJJJ HH:MM", bad.message.c_str());
    TEST_ASSERT_EQUAL_INT(1, fake.calls);
}

void test_tf10_tf11_entry_error_replies()
{
    fake.reply = {409, "{\"error\":\"already_open\"}"};
    TEST_ASSERT_EQUAL_STRING("Parkvorgang bereits offen – bitte Ruftaste drücken",
                             entryRequest(regs, "ZH98765", "", fake).message.c_str());
    TEST_ASSERT_FALSE(entryRequest(regs, "ZH98765", "", fake).unreachable);
    fake.reply = {0, ""};
    GateResult r = entryRequest(regs, "LU123456", "", fake);
    TEST_ASSERT_FALSE(r.open);
    TEST_ASSERT_TRUE(r.unreachable);
    TEST_ASSERT_EQUAL_STRING("Kasse nicht erreichbar – bitte Ruftaste drücken", r.message.c_str());
    fake.reply = {400, "{\"error\":\"entry_in_future\"}"};
    TEST_ASSERT_EQUAL_STRING("Einfahrtszeit liegt in der Zukunft",
                             entryRequest(regs, "LU123456", "", fake).message.c_str());
    fake.reply = {503, "{\"error\":\"time_not_synced\"}"};
    TEST_ASSERT_FALSE(entryRequest(regs, "LU123456", "", fake).open);
}

void test_entry_fail_safe_on_odd_replies()
{
    const HttpReply odd[] = {
        {201, "kaputt"},
        {201, "{\"kz\":\"ZH98765\",\"entry\":1}"},
        {201, "{\"kz\":\"LU123456\"}"},
        {200, "{\"kz\":\"LU123456\",\"entry\":1}"},
        {500, ""},
        {-1, ""},
    };
    for (const HttpReply& reply : odd)
    {
        fake.reply = reply;
        TEST_ASSERT_FALSE(entryRequest(regs, "LU123456", "", fake).open);
    }
}

void test_tf23_exit_paid_opens()
{
    fake.reply = {200, "{\"kz\":\"LU123456\",\"status\":\"closed\",\"exit\":1760533600,\"fee\":400}"};
    GateResult r = exitRequest(" LU123456 ", fake);
    TEST_ASSERT_TRUE(r.open);
    TEST_ASSERT_EQUAL_STRING("LU123456 – Gute Fahrt, Ausfahrt frei", r.message.c_str());
    TEST_ASSERT_EQUAL_STRING("/api/exit", fake.lastPath.c_str());
    TEST_ASSERT_EQUAL_STRING("{\"kz\":\"LU123456\"}", fake.lastBody.c_str());
    fake.reply = {200, "{\"kz\":\"LU123456\",\"status\":\"closed_free\",\"exit\":1760533600,\"fee\":0}"};
    TEST_ASSERT_TRUE(exitRequest("LU123456", fake).open);
}

void test_tf24_tf26_tf30_exit_unpaid_and_errors()
{
    fake.reply = {402, "{\"error\":\"unpaid\",\"fee\":600}"};
    GateResult unpaid = exitRequest("ZH98765", fake);
    TEST_ASSERT_FALSE(unpaid.open);
    TEST_ASSERT_EQUAL_STRING("Gebühr CHF 6.00 nicht bezahlt – bitte am Automaten bezahlen", unpaid.message.c_str());
    TEST_ASSERT_TRUE(unpaid.unpaid);
    TEST_ASSERT_EQUAL_UINT32(600, unpaid.feeRappen);
    fake.reply = {404, "{\"error\":\"no_session\"}"};
    TEST_ASSERT_EQUAL_STRING("Kein Parkvorgang gefunden – bitte Ruftaste drücken",
                             exitRequest("SZ555", fake).message.c_str());
    fake.reply = {0, ""};
    TEST_ASSERT_EQUAL_STRING("Kasse nicht erreichbar – bitte Ruftaste drücken",
                             exitRequest("LU123456", fake).message.c_str());
}

void test_exit_fail_safe_on_odd_replies()
{
    const HttpReply odd[] = {
        {200, ""},
        {200, "{\"kz\":\"LU123456\",\"status\":\"open\"}"},
        {200, "{\"kz\":\"ZH98765\",\"status\":\"closed\"}"},
        {200, "[\"closed\"]"},
        {402, "{\"error\":\"unpaid\"}"},
        {201, "{\"kz\":\"LU123456\",\"status\":\"closed\"}"},
    };
    for (const HttpReply& reply : odd)
    {
        fake.reply = reply;
        TEST_ASSERT_FALSE(exitRequest("LU123456", fake).open);
    }
}

void test_against_real_kasse_handlers()
{
    LocalKasse kasse;
    TEST_ASSERT_TRUE(entryRequest(regs, "ZH 98765", "", kasse).open);
    TEST_ASSERT_TRUE(entryRequest(regs, "ZH98765", "", kasse).open); // repeated entry (late answer + retry)
    kasse.now += REPEAT_ENTRY_SECONDS;
    TEST_ASSERT_FALSE(entryRequest(regs, "ZH98765", "", kasse).open);
    kasse.now += 3600 - REPEAT_ENTRY_SECONDS;
    GateResult unpaid = exitRequest("ZH98765", kasse);
    TEST_ASSERT_EQUAL_STRING("Gebühr CHF 2.00 nicht bezahlt – bitte am Automaten bezahlen", unpaid.message.c_str());
    TEST_ASSERT_TRUE(entryRequest(regs, "AG1", "", kasse).open);
    kasse.now += 5 * 60;
    TEST_ASSERT_TRUE(exitRequest("AG1", kasse).open);
    TEST_ASSERT_EQUAL(SessionStatus::ClosedFree, kasse.store.findLatest("AG1")->status);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_tf04_tf05_entry_registered_opens);
    RUN_TEST(test_tf07_entry_unregistered_stays_closed_without_request);
    RUN_TEST(test_entry_invalid_plate_no_request);
    RUN_TEST(test_entry_backdated_sends_epoch);
    RUN_TEST(test_tf10_tf11_entry_error_replies);
    RUN_TEST(test_entry_fail_safe_on_odd_replies);
    RUN_TEST(test_tf23_exit_paid_opens);
    RUN_TEST(test_tf24_tf26_tf30_exit_unpaid_and_errors);
    RUN_TEST(test_exit_fail_safe_on_odd_replies);
    RUN_TEST(test_against_real_kasse_handlers);
    return UNITY_END();
}
