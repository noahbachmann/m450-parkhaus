#include <unity.h>

#include "testdata.h"

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
    TEST_ASSERT_TRUE(list.contains("LU123456"));
    TEST_ASSERT_TRUE(list.contains("ZH98765"));
    TEST_ASSERT_TRUE(list.contains("AG1"));
    TEST_ASSERT_TRUE(list.contains("SZ555"));
    TEST_ASSERT_FALSE(list.contains("OW2026"));
    TEST_ASSERT_FALSE(list.contains("BE4711"));
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
