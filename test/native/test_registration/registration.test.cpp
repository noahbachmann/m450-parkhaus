#include <unity.h>

#include <string>

#include "registration.h"

void setUp()
{
}

void tearDown()
{
}

void test_add_normalizes_plate_and_trims_name()
{
    RegistrationList list;
    TEST_ASSERT_TRUE(RegistrationResult::Ok == list.add("  Test Kunde C ", "NW 654321"));
    TEST_ASSERT_EQUAL(1, list.all().size());
    TEST_ASSERT_EQUAL_STRING("Test Kunde C", list.all()[0].name.c_str());
    TEST_ASSERT_EQUAL_STRING("NW654321", list.all()[0].plate.c_str());
    TEST_ASSERT_TRUE(list.contains("nw 654 321"));
}

void test_invalid_plates_are_rejected()
{
    RegistrationList list;
    const char* invalid[] = {"", "LU-12#45", "LU 1234567", "L 123"};
    for (const char* plate : invalid)
    {
        TEST_ASSERT_TRUE_MESSAGE(RegistrationResult::InvalidPlate == list.add("Test Kunde", plate), plate);
    }
    TEST_ASSERT_EQUAL(0, list.all().size());
    TEST_ASSERT_TRUE(RegistrationResult::Ok == list.add("Test Kunde", "UR 1"));
    TEST_ASSERT_TRUE(list.contains("UR1"));
}

void test_invalid_names_are_rejected()
{
    RegistrationList list;
    TEST_ASSERT_TRUE(RegistrationResult::InvalidName == list.add("", "UR 1"));
    TEST_ASSERT_TRUE(RegistrationResult::InvalidName == list.add("   ", "UR 1"));
    TEST_ASSERT_TRUE(RegistrationResult::InvalidName == list.add(std::string(MAX_NAME_BYTES + 1, 'x'), "UR 1"));
    TEST_ASSERT_TRUE(RegistrationResult::Ok == list.add(std::string(MAX_NAME_BYTES, 'x'), "UR 1"));
}

void test_duplicate_plate_is_rejected()
{
    RegistrationList list;
    TEST_ASSERT_TRUE(RegistrationResult::Ok == list.add("Test Kunde A", "LU 123456"));
    TEST_ASSERT_TRUE(RegistrationResult::Duplicate == list.add("Test Kunde B", "LU 123456"));
    TEST_ASSERT_TRUE(RegistrationResult::Duplicate == list.add("Test Kunde B", "lu123456"));
    TEST_ASSERT_EQUAL(1, list.all().size());
    TEST_ASSERT_EQUAL_STRING("Test Kunde A", list.all()[0].name.c_str());
}

void test_remove()
{
    RegistrationList list;
    list.add("Test Kunde A", "LU 123456");
    list.add("Test Kunde B", "ZH 98765");
    TEST_ASSERT_FALSE(list.remove("BE 4711"));
    TEST_ASSERT_TRUE(list.remove("lu 123456"));
    TEST_ASSERT_FALSE(list.contains("LU123456"));
    TEST_ASSERT_TRUE(list.contains("ZH98765"));
    TEST_ASSERT_TRUE(RegistrationResult::Ok == list.add("Test Kunde C", "LU123456"));
}

void test_list_is_capped()
{
    RegistrationList list;
    for (size_t i = 0; i < MAX_REGISTRATIONS; ++i)
    {
        TEST_ASSERT_TRUE(RegistrationResult::Ok == list.add("Kunde", "AG" + std::to_string(i + 1)));
    }
    TEST_ASSERT_TRUE(RegistrationResult::Full == list.add("Kunde", "UR 1"));
    TEST_ASSERT_EQUAL(MAX_REGISTRATIONS, list.all().size());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_add_normalizes_plate_and_trims_name);
    RUN_TEST(test_invalid_plates_are_rejected);
    RUN_TEST(test_invalid_names_are_rejected);
    RUN_TEST(test_duplicate_plate_is_rejected);
    RUN_TEST(test_remove);
    RUN_TEST(test_list_is_capped);
    return UNITY_END();
}
