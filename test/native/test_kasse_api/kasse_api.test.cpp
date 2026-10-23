#include <unity.h>

#include <string>

#include "kasse_api.h"
#include "payment.h"

namespace
{
    constexpr int64_t NOW = 1760530000;
    constexpr int64_t MIN = 60;
    constexpr int64_t HOUR = 3600;

    void assertResponse(int status, const char* body, const ApiResponse& r)
    {
        TEST_ASSERT_EQUAL_INT_MESSAGE(status, r.status, r.body.c_str());
        TEST_ASSERT_EQUAL_STRING(body, r.body.c_str());
    }

    // KZ-01 paid, KZ-02 unpaid (CHF 6.00), KZ-08 in free time, KZ-10 no session.
    SessionStore testData()
    {
        SessionStore store;
        store.openEntry("LU 123456", NOW - 80 * MIN);
        pay(store, "LU123456", PayMethod::CardOk, NOW - MIN);
        store.openEntry("ZH 98765", NOW - (2 * HOUR + 30 * MIN));
        store.openEntry("AG 1", NOW - 5 * MIN);
        return store;
    }
}

void setUp()
{
}

void tearDown()
{
}

void test_status_tf29()
{
    SessionStore store = testData();
    assertResponse(200, "{\"kz\":\"LU123456\",\"status\":\"bezahlt\",\"fee\":400,\"entry\":1760525200}",
                   handleStatus(store, "LU123456", NOW));
    assertResponse(200, "{\"kz\":\"ZH98765\",\"status\":\"unbezahlt\",\"fee\":600,\"entry\":1760521000}",
                   handleStatus(store, "ZH 98765", NOW));
    assertResponse(200, "{\"kz\":\"SZ555\",\"status\":\"unbekannt\"}", handleStatus(store, "SZ555", NOW));
    assertResponse(400, "{\"error\":\"invalid_plate\"}", handleStatus(store, "", NOW));
    assertResponse(400, "{\"error\":\"invalid_plate\"}", handleStatus(store, "LU-12#45", NOW));
}

void test_status_open_in_free_time_is_unpaid_with_fee_zero()
{
    SessionStore store = testData();
    assertResponse(200, "{\"kz\":\"AG1\",\"status\":\"unbezahlt\",\"fee\":0,\"entry\":1760529700}",
                   handleStatus(store, "AG1", NOW));
}

void test_status_after_exit_is_unknown()
{
    SessionStore store = testData();
    store.tryExit("LU123456", NOW);
    assertResponse(200, "{\"kz\":\"LU123456\",\"status\":\"unbekannt\"}", handleStatus(store, "LU123456", NOW));
}

void test_entry()
{
    SessionStore store;
    assertResponse(201, "{\"kz\":\"LU123456\",\"entry\":1760530000}",
                   handleEntry(store, "{\"kz\":\"lu 123 456\"}", NOW, true));
    assertResponse(409, "{\"error\":\"already_open\"}", handleEntry(store, "{\"kz\":\"LU123456\"}", NOW + MIN, true));
    TEST_ASSERT_EQUAL(1, store.all().size());
}

void test_entry_backdated()
{
    SessionStore store;
    assertResponse(201, "{\"kz\":\"OW2026\",\"entry\":1760521000}",
                   handleEntry(store, "{\"kz\":\"OW 2026\",\"entry\":1760521000}", NOW, true));
    assertResponse(400, "{\"error\":\"entry_in_future\"}",
                   handleEntry(store, "{\"kz\":\"UR1\",\"entry\":1760533600}", NOW, true));
    TEST_ASSERT_NULL(store.findActive("UR1"));
}

void test_entry_errors()
{
    SessionStore store;
    assertResponse(400, "{\"error\":\"invalid_plate\"}", handleEntry(store, "{\"kz\":\"L 123\"}", NOW, true));
    assertResponse(400, "{\"error\":\"invalid_plate\"}", handleEntry(store, "{\"kz\":\"\"}", NOW, true));
    assertResponse(503, "{\"error\":\"time_not_synced\"}", handleEntry(store, "{\"kz\":\"UR1\"}", NOW, false));
    const char* bad[] = {"", "{", "[]", "\"UR1\"", "{}", "{\"kz\":1}", "{\"kz\":\"UR1\",\"entry\":\"yesterday\"}"};
    for (const char* body : bad)
    {
        ApiResponse r = handleEntry(store, body, NOW, true);
        TEST_ASSERT_EQUAL_INT_MESSAGE(400, r.status, body);
        TEST_ASSERT_EQUAL_STRING_MESSAGE("{\"error\":\"bad_request\"}", r.body.c_str(), body);
    }
    TEST_ASSERT_EQUAL(0, store.all().size());
}

void test_exit()
{
    SessionStore store = testData();
    assertResponse(200, "{\"kz\":\"LU123456\",\"status\":\"closed\",\"exit\":1760530000,\"fee\":400}",
                   handleExit(store, "{\"kz\":\" LU123456 \"}", NOW, true));
    assertResponse(200, "{\"kz\":\"AG1\",\"status\":\"closed_free\",\"exit\":1760530000,\"fee\":0}",
                   handleExit(store, "{\"kz\":\"AG 1\"}", NOW, true));
    assertResponse(402, "{\"error\":\"unpaid\",\"fee\":600}", handleExit(store, "{\"kz\":\"ZH98765\"}", NOW, true));
    TEST_ASSERT_TRUE(SessionStatus::Open == store.findActive("ZH98765")->status);
    assertResponse(404, "{\"error\":\"no_session\"}", handleExit(store, "{\"kz\":\"SZ555\"}", NOW, true));
    assertResponse(404, "{\"error\":\"no_session\"}", handleExit(store, "{\"kz\":\"LU123456\"}", NOW, true));
}

void test_exit_errors()
{
    SessionStore store = testData();
    assertResponse(400, "{\"error\":\"invalid_plate\"}", handleExit(store, "{\"kz\":\"LU-12#45\"}", NOW, true));
    assertResponse(400, "{\"error\":\"bad_request\"}", handleExit(store, "not json", NOW, true));
    assertResponse(503, "{\"error\":\"time_not_synced\"}", handleExit(store, "{\"kz\":\"LU123456\"}", NOW, false));
    assertResponse(503, "{\"error\":\"time_not_synced\"}",
                   handleExit(store, "{\"kz\":\"ZH98765\"}", NOW - 3 * HOUR, true));
    TEST_ASSERT_TRUE(SessionStatus::Paid == store.findActive("LU123456")->status);
}

void test_exit_override_tf31()
{
    SessionStore store = testData();
    assertResponse(200, "{\"kz\":\"ZH98765\",\"status\":\"closed_unpaid\",\"exit\":1760530000,\"fee\":600}",
                   handleExit(store, "{\"kz\":\"ZH98765\",\"override\":true}", NOW, true));
    assertResponse(200, "{\"kz\":\"LU123456\",\"status\":\"closed\",\"exit\":1760530000,\"fee\":400}",
                   handleExit(store, "{\"kz\":\"LU123456\",\"override\":true}", NOW, true));
    assertResponse(404, "{\"error\":\"no_session\"}", handleExit(store, "{\"kz\":\"SZ555\",\"override\":true}", NOW, true));
    store.openEntry("UR1", NOW - 3 * HOUR);
    assertResponse(402, "{\"error\":\"unpaid\",\"fee\":600}",
                   handleExit(store, "{\"kz\":\"UR1\",\"override\":false}", NOW, true));
    assertResponse(400, "{\"error\":\"bad_request\"}", handleExit(store, "{\"kz\":\"UR1\",\"override\":1}", NOW, true));
    TEST_ASSERT_TRUE(SessionStatus::Open == store.findActive("UR1")->status);
}

void test_pay_at_exit_terminal_tf28()
{
    SessionStore store = testData();
    assertResponse(402, "{\"error\":\"declined\"}",
                   handlePay(store, "{\"kz\":\"ZH98765\",\"method\":\"card_declined\"}", NOW, true));
    TEST_ASSERT_TRUE(SessionStatus::Open == store.findActive("ZH98765")->status);
    assertResponse(200, "{\"kz\":\"ZH98765\",\"status\":\"bezahlt\",\"amount\":600,\"method\":\"twint\",\"tx\":\"TX-000002\"}",
                   handlePay(store, "{\"kz\":\"zh 98765\",\"method\":\"twint\"}", NOW, true));
    assertResponse(409, "{\"error\":\"already_paid\"}",
                   handlePay(store, "{\"kz\":\"ZH98765\",\"method\":\"card_ok\"}", NOW, true));
    assertResponse(409, "{\"error\":\"nothing_to_pay\"}",
                   handlePay(store, "{\"kz\":\"AG1\",\"method\":\"card_ok\"}", NOW, true));
    assertResponse(404, "{\"error\":\"no_session\"}",
                   handlePay(store, "{\"kz\":\"SZ555\",\"method\":\"card_ok\"}", NOW, true));
    assertResponse(400, "{\"error\":\"bad_request\"}",
                   handlePay(store, "{\"kz\":\"SZ555\",\"method\":\"cash\"}", NOW, true));
    assertResponse(400, "{\"error\":\"bad_request\"}", handlePay(store, "{\"kz\":\"SZ555\"}", NOW, true));
    assertResponse(400, "{\"error\":\"invalid_plate\"}",
                   handlePay(store, "{\"kz\":\"L1\",\"method\":\"twint\"}", NOW, true));
    assertResponse(503, "{\"error\":\"time_not_synced\"}",
                   handlePay(store, "{\"kz\":\"AG1\",\"method\":\"twint\"}", NOW, false));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_status_tf29);
    RUN_TEST(test_status_open_in_free_time_is_unpaid_with_fee_zero);
    RUN_TEST(test_status_after_exit_is_unknown);
    RUN_TEST(test_entry);
    RUN_TEST(test_entry_backdated);
    RUN_TEST(test_entry_errors);
    RUN_TEST(test_exit);
    RUN_TEST(test_exit_errors);
    RUN_TEST(test_exit_override_tf31);
    RUN_TEST(test_pay_at_exit_terminal_tf28);
    return UNITY_END();
}
