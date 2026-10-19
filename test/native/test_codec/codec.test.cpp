#include <unity.h>

#include <string>

#include "codec.h"
#include "payment.h"

namespace
{
    constexpr int64_t NOW = 1760530000;
    constexpr int64_t MIN = 60;
    constexpr int64_t HOUR = 3600;

    SessionStore sampleStore()
    {
        SessionStore store;
        store.openEntry("LU 123456", NOW - 80 * MIN);
        pay(store, "LU123456", PayMethod::CardOk, NOW);
        store.openEntry("ZH 98765", NOW - 2 * HOUR);
        pay(store, "ZH98765", PayMethod::Cash, NOW, 100);
        store.openEntry("AG 1", NOW - 5 * MIN);
        store.tryExit("AG1", NOW);
        store.openEntry("OW 2026", NOW - HOUR);
        pay(store, "OW2026", PayMethod::Twint, NOW);
        store.tryExit("OW2026", NOW + MIN);
        return store;
    }

    void assertSameSession(const Session& a, const Session& b)
    {
        TEST_ASSERT_EQUAL_STRING(a.plate.c_str(), b.plate.c_str());
        TEST_ASSERT_EQUAL_INT64(a.entry, b.entry);
        TEST_ASSERT_EQUAL_INT64(a.exit, b.exit);
        TEST_ASSERT_EQUAL_UINT32(a.amountRappen, b.amountRappen);
        TEST_ASSERT_EQUAL_UINT32(a.cashPaidRappen, b.cashPaidRappen);
        TEST_ASSERT_TRUE(a.method == b.method);
        TEST_ASSERT_EQUAL_STRING(a.txRef.c_str(), b.txRef.c_str());
        TEST_ASSERT_TRUE(a.status == b.status);
    }
}

void setUp()
{
}

void tearDown()
{
}

void test_registrations_round_trip()
{
    RegistrationList list;
    list.add("Test Kunde A", "LU 123456");
    list.add("Jörg Müller", "ZH 98765");
    std::string json = registrationsToJson(list);

    RegistrationList loaded;
    TEST_ASSERT_TRUE(registrationsFromJson(json, loaded));
    TEST_ASSERT_EQUAL(2, loaded.all().size());
    TEST_ASSERT_EQUAL_STRING("Test Kunde A", loaded.all()[0].name.c_str());
    TEST_ASSERT_EQUAL_STRING("LU123456", loaded.all()[0].plate.c_str());
    TEST_ASSERT_EQUAL_STRING("Jörg Müller", loaded.all()[1].name.c_str());
}

void test_registrations_format()
{
    RegistrationList list;
    list.add("Test Kunde A", "LU 123456");
    TEST_ASSERT_EQUAL_STRING("{\"registrations\":[{\"name\":\"Test Kunde A\",\"kz\":\"LU123456\"}]}",
                             registrationsToJson(list).c_str());
}

void test_registrations_invalid_input_leaves_list_unchanged()
{
    RegistrationList list;
    list.add("Test Kunde A", "LU 123456");
    const char* bad[] = {
        "",
        "{",
        "[]",
        "{\"registrations\":{}}",
        "{\"registrations\":[{\"name\":\"X\"}]}",
        "{\"registrations\":[{\"name\":\"X\",\"kz\":\"LU-12#45\"}]}",
        "{\"registrations\":[{\"name\":\"X\",\"kz\":\"UR1\"},{\"name\":\"Y\",\"kz\":\"ur 1\"}]}",
        "{\"registrations\":[{\"name\":\"X\",\"kz\":42}]}",
    };
    for (const char* json : bad)
    {
        TEST_ASSERT_FALSE_MESSAGE(registrationsFromJson(json, list), json);
    }
    TEST_ASSERT_EQUAL(1, list.all().size());
    TEST_ASSERT_TRUE(registrationsFromJson("{\"registrations\":[]}", list));
    TEST_ASSERT_EQUAL(0, list.all().size());
}

void test_sessions_round_trip()
{
    SessionStore store = sampleStore();
    std::string json = sessionsToJson(store);

    SessionStore loaded;
    TEST_ASSERT_TRUE(sessionsFromJson(json, loaded));
    TEST_ASSERT_EQUAL(store.all().size(), loaded.all().size());
    for (size_t i = 0; i < store.all().size(); ++i)
    {
        assertSameSession(store.all()[i], loaded.all()[i]);
    }
    TEST_ASSERT_EQUAL_UINT32(store.nextTxNumber(), loaded.nextTxNumber());
    TEST_ASSERT_EQUAL_STRING("TX-000003", loaded.takeTxRef().c_str());
}

void test_sessions_format()
{
    SessionStore store;
    store.openEntry("LU 123456", NOW - 80 * MIN);
    pay(store, "LU123456", PayMethod::CardOk, NOW);
    TEST_ASSERT_EQUAL_STRING("{\"nextTx\":2,\"sessions\":[{\"kz\":\"LU123456\",\"entry\":1760525200,\"exit\":0,"
                             "\"amount\":400,\"cash\":0,\"method\":\"card\",\"tx\":\"TX-000001\","
                             "\"status\":\"paid\"}]}",
                             sessionsToJson(store).c_str());
}

void test_sessions_invalid_input_leaves_store_unchanged()
{
    SessionStore store = sampleStore();
    std::string before = sessionsToJson(store);
    const std::string ok = "{\"kz\":\"LU123456\",\"entry\":1,\"exit\":0,\"amount\":0,\"cash\":0,"
                           "\"method\":\"none\",\"tx\":\"\",\"status\":\"open\"}";
    const std::string bad[] = {
        "",
        "null",
        "{\"sessions\":[]}",
        "{\"nextTx\":1}",
        "{\"nextTx\":-1,\"sessions\":[]}",
        "{\"nextTx\":1,\"sessions\":[{\"kz\":\"LU123456\"}]}",
        "{\"nextTx\":1,\"sessions\":[" + ok + "," + ok + "]}",
        "{\"nextTx\":1,\"sessions\":[{\"kz\":\"LU123456\",\"entry\":1,\"exit\":0,\"amount\":0,\"cash\":0,"
        "\"method\":\"none\",\"tx\":\"\",\"status\":\"weird\"}]}",
        "{\"nextTx\":1,\"sessions\":[{\"kz\":\"L1\",\"entry\":1,\"exit\":0,\"amount\":0,\"cash\":0,"
        "\"method\":\"none\",\"tx\":\"\",\"status\":\"open\"}]}",
        "{\"nextTx\":1,\"sessions\":[{\"kz\":\"LU123456\",\"entry\":\"1\",\"exit\":0,\"amount\":0,\"cash\":0,"
        "\"method\":\"none\",\"tx\":\"\",\"status\":\"open\"}]}",
    };
    for (const std::string& json : bad)
    {
        TEST_ASSERT_FALSE_MESSAGE(sessionsFromJson(json, store), json.c_str());
    }
    TEST_ASSERT_EQUAL_STRING(before.c_str(), sessionsToJson(store).c_str());
    TEST_ASSERT_TRUE(sessionsFromJson("{\"nextTx\":1,\"sessions\":[" + ok + "]}", store));
    TEST_ASSERT_EQUAL(1, store.all().size());
}

void test_status_names()
{
    TEST_ASSERT_EQUAL_STRING("open", sessionStatusName(SessionStatus::Open));
    TEST_ASSERT_EQUAL_STRING("paid", sessionStatusName(SessionStatus::Paid));
    TEST_ASSERT_EQUAL_STRING("closed", sessionStatusName(SessionStatus::Closed));
    TEST_ASSERT_EQUAL_STRING("closed_free", sessionStatusName(SessionStatus::ClosedFree));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_registrations_round_trip);
    RUN_TEST(test_registrations_format);
    RUN_TEST(test_registrations_invalid_input_leaves_list_unchanged);
    RUN_TEST(test_sessions_round_trip);
    RUN_TEST(test_sessions_format);
    RUN_TEST(test_sessions_invalid_input_leaves_store_unchanged);
    RUN_TEST(test_status_names);
    return UNITY_END();
}
