#include <unity.h>

#include "plate.h"

void setUp()
{
}

void tearDown()
{
}

void test_normalize_removes_spaces_and_uppercases()
{
    TEST_ASSERT_EQUAL_STRING("LU123456", normalizePlate("lu 123 456").c_str());
    TEST_ASSERT_EQUAL_STRING("LU123456", normalizePlate(" LU123456 ").c_str());
    TEST_ASSERT_EQUAL_STRING("LU123456", normalizePlate("\tLu 123\t456\n").c_str());
    TEST_ASSERT_EQUAL_STRING("NW654321", normalizePlate("NW 654321").c_str());
    TEST_ASSERT_EQUAL_STRING("", normalizePlate("   ").c_str());
}

void test_normalize_keeps_special_chars()
{
    TEST_ASSERT_EQUAL_STRING("LU-12#45", normalizePlate("LU-12#45").c_str());
}

void test_valid_plates()
{
    const char* valid[] = {"LU123456", "ZH98765", "BE4711", "AG1", "UR1", "NW654321", "OW2026", "SZ555"};
    for (const char* plate : valid)
    {
        TEST_ASSERT_TRUE_MESSAGE(isValidPlate(normalizePlate(plate)), plate);
    }
}

void test_invalid_plates()
{
    const char* invalid[] = {"", "LU-12#45", "LU 1234567", "L 123", "LU", "LUZ1", "12AB", "L1", "LU12A"};
    for (const char* plate : invalid)
    {
        TEST_ASSERT_FALSE_MESSAGE(isValidPlate(normalizePlate(plate)), plate);
    }
}

void test_unnormalized_input_is_invalid()
{
    TEST_ASSERT_FALSE(isValidPlate("lu123456"));
    TEST_ASSERT_FALSE(isValidPlate("LU 123456"));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_normalize_removes_spaces_and_uppercases);
    RUN_TEST(test_normalize_keeps_special_chars);
    RUN_TEST(test_valid_plates);
    RUN_TEST(test_invalid_plates);
    RUN_TEST(test_unnormalized_input_is_invalid);
    return UNITY_END();
}
