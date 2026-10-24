#include <unity.h>

#include "test_fixtures.h"
#include "testdata.h"

using namespace fixtures;

void setUp()
{
}

void tearDown()
{
}

void test_default_registrations_match_kz_table()
{
    RegistrationList list;
    list.add("Vorher", "OW 2026");
    loadDefaultRegistrations(list);
    TEST_ASSERT_EQUAL(4, list.all().size());
    for (const char* kz : {KZ_01, KZ_02, KZ_08, KZ_10})
    {
        TEST_ASSERT_TRUE_MESSAGE(list.contains(kz), kz);
    }
    for (const char* kz : {KZ_03, KZ_11, KZ_12, KZ_13})
    {
        TEST_ASSERT_FALSE_MESSAGE(list.contains(kz), kz);
    }
    TEST_ASSERT_EQUAL_STRING("Test Kunde A", list.all()[0].name.c_str()); // KZ-01, TF-03
    TEST_ASSERT_EQUAL_STRING("Test Kunde B", list.all()[1].name.c_str()); // KZ-02
}

void test_default_sessions_are_empty()
{
    SessionStore store;
    store.openEntry("LU 123456", 1760000000);
    store.takeTxRef();
    loadDefaultSessions(store);
    TEST_ASSERT_EQUAL(0, store.all().size());
    TEST_ASSERT_EQUAL(1, store.nextTxNumber());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_default_registrations_match_kz_table);
    RUN_TEST(test_default_sessions_are_empty);
    return UNITY_END();
}
