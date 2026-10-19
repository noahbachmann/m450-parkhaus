#include <unity.h>

#include "tariff.h"

namespace
{
    constexpr int64_t ENTRY = 1760530000;
    constexpr int64_t MIN = 60;
    constexpr int64_t HOUR = 3600;

    struct Case
    {
        const char* id;
        int64_t duration;
        uint32_t expectedRappen;
    };

    void checkCases(const Case* cases, size_t count)
    {
        for (size_t i = 0; i < count; ++i)
        {
            uint32_t fee = 12345;
            TEST_ASSERT_TRUE_MESSAGE(calculateFee(ENTRY, ENTRY + cases[i].duration, fee), cases[i].id);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(cases[i].expectedRappen, fee, cases[i].id);
        }
    }
}

void setUp()
{
}

void tearDown()
{
}

void test_reference_values()
{
    const Case cases[] = {
        {"R-01", 0, 0},
        {"R-02", 15 * MIN, 0},
        {"R-03", 15 * MIN + 1, 200},
        {"R-04", 1 * HOUR, 200},
        {"R-05", 1 * HOUR + 1, 400},
        {"R-06", 2 * HOUR + 30 * MIN, 600},
        {"R-07", 10 * HOUR, 2000},
        {"R-08", 10 * HOUR + 1, 2000},
        {"R-09", 24 * HOUR, 2000},
        {"R-10", 24 * HOUR + 1, 2200},
        {"R-11", 48 * HOUR, 4000},
    };
    checkCases(cases, sizeof(cases) / sizeof(cases[0]));
}

void test_r12_entry_in_future_is_rejected()
{
    uint32_t fee = 12345;
    TEST_ASSERT_FALSE(calculateFee(ENTRY + HOUR, ENTRY, fee));
    TEST_ASSERT_FALSE(calculateFee(ENTRY + 1, ENTRY, fee));
}

void test_other_values_from_test_concept()
{
    const Case cases[] = {
        {"TF-19 1h20", 1 * HOUR + 20 * MIN, 400},
        {"TF-27 58min", 58 * MIN, 200},
        {"TF-27 61min", 61 * MIN, 400},
        {"TF-25 5min", 5 * MIN, 0},
    };
    checkCases(cases, sizeof(cases) / sizeof(cases[0]));
}

void test_free_minutes_do_not_apply_in_later_day_blocks()
{
    const Case cases[] = {
        {"24h + 5min", 24 * HOUR + 5 * MIN, 2200},
        {"24h + 1h01", 25 * HOUR + 1, 2400},
        {"72h", 72 * HOUR, 6000},
    };
    checkCases(cases, sizeof(cases) / sizeof(cases[0]));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_reference_values);
    RUN_TEST(test_r12_entry_in_future_is_rejected);
    RUN_TEST(test_other_values_from_test_concept);
    RUN_TEST(test_free_minutes_do_not_apply_in_later_day_blocks);
    return UNITY_END();
}
