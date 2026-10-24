#include <unity.h>

#include "format.h"

namespace
{
    // 2026-03-29 01:00:00 UTC: CEST starts. 2026-10-25 01:00:00 UTC: CEST ends.
    constexpr int64_t CEST_START_2026 = 1774746000;
    constexpr int64_t CEST_END_2026 = 1792890000;
}

void setUp()
{
}

void tearDown()
{
}

void test_chf()
{
    TEST_ASSERT_EQUAL_STRING("CHF 0.00", formatChf(0).c_str());
    TEST_ASSERT_EQUAL_STRING("CHF 0.05", formatChf(5).c_str());
    TEST_ASSERT_EQUAL_STRING("CHF 4.00", formatChf(400).c_str());
    TEST_ASSERT_EQUAL_STRING("CHF 22.50", formatChf(2250).c_str());
    TEST_ASSERT_EQUAL_STRING("CHF 1234.99", formatChf(123499).c_str());
}

void test_zurich_offset_at_dst_boundaries()
{
    TEST_ASSERT_EQUAL_INT32(3600, zurichOffsetSeconds(CEST_START_2026 - 1));
    TEST_ASSERT_EQUAL_INT32(7200, zurichOffsetSeconds(CEST_START_2026));
    TEST_ASSERT_EQUAL_INT32(7200, zurichOffsetSeconds(CEST_END_2026 - 1));
    TEST_ASSERT_EQUAL_INT32(3600, zurichOffsetSeconds(CEST_END_2026));
    TEST_ASSERT_EQUAL_INT32(3600, zurichOffsetSeconds(1767225600));
}

void test_date_time_local()
{
    TEST_ASSERT_EQUAL_STRING("15.10.2025 14:06:40", formatDateTime(1760530000).c_str());
    TEST_ASSERT_EQUAL_STRING("01.01.2026 01:00:00", formatDateTime(1767225600).c_str());
    TEST_ASSERT_EQUAL_STRING("01.03.2024 00:00:00", formatDateTime(1709247600).c_str());
    TEST_ASSERT_EQUAL_STRING("01.01.1970 01:00:00", formatDateTime(0).c_str());
}

void test_date_time_across_dst_switch()
{
    TEST_ASSERT_EQUAL_STRING("29.03.2026 01:59:59", formatDateTime(CEST_START_2026 - 1).c_str());
    TEST_ASSERT_EQUAL_STRING("29.03.2026 03:00:00", formatDateTime(CEST_START_2026).c_str());
    TEST_ASSERT_EQUAL_STRING("25.10.2026 02:59:59", formatDateTime(CEST_END_2026 - 1).c_str());
    TEST_ASSERT_EQUAL_STRING("25.10.2026 02:00:00", formatDateTime(CEST_END_2026).c_str());
}

void test_duration()
{
    TEST_ASSERT_EQUAL_STRING("1 h 20 min", formatDuration(80 * 60).c_str());
    TEST_ASSERT_EQUAL_STRING("1 h 20 min", formatDuration(80 * 60 + 59).c_str());
    TEST_ASSERT_EQUAL_STRING("0 h 5 min", formatDuration(5 * 60).c_str());
    TEST_ASSERT_EQUAL_STRING("0 h 0 min", formatDuration(59).c_str());
    TEST_ASSERT_EQUAL_STRING("26 h 3 min", formatDuration(26 * 3600 + 3 * 60).c_str());
    TEST_ASSERT_EQUAL_STRING("0 h 0 min", formatDuration(-10).c_str());
}

void test_parse_local_date_time()
{
    int64_t e = 0;
    TEST_ASSERT_TRUE(parseLocalDateTime("2025-10-15T14:06:40", e));
    TEST_ASSERT_EQUAL_INT64(1760530000, e);
    TEST_ASSERT_TRUE(parseLocalDateTime(" 15.10.2025 14:06:40 ", e));
    TEST_ASSERT_EQUAL_INT64(1760530000, e);
    TEST_ASSERT_TRUE(parseLocalDateTime("2025-10-15T14:06", e));
    TEST_ASSERT_EQUAL_INT64(1760530000 - 40, e);
    TEST_ASSERT_TRUE(parseLocalDateTime("01.01.2026 01:00", e));
    TEST_ASSERT_EQUAL_INT64(1767225600, e);
}

void test_parse_local_date_time_at_dst_switch()
{
    int64_t e = 0;
    TEST_ASSERT_TRUE(parseLocalDateTime("29.03.2026 03:00:00", e));
    TEST_ASSERT_EQUAL_INT64(CEST_START_2026, e);
    TEST_ASSERT_TRUE(parseLocalDateTime("29.03.2026 02:30", e)); // gap: read as CET
    TEST_ASSERT_EQUAL_INT64(CEST_START_2026 + 1800, e);
    TEST_ASSERT_TRUE(parseLocalDateTime("25.10.2026 02:30", e)); // overlap: CEST
    TEST_ASSERT_EQUAL_INT64(CEST_END_2026 - 1800, e);
    TEST_ASSERT_TRUE(parseLocalDateTime("25.10.2026 03:00", e));
    TEST_ASSERT_EQUAL_INT64(CEST_END_2026 + 3600, e);
}

void test_parse_local_date_time_rejects()
{
    int64_t e = 7;
    TEST_ASSERT_FALSE(parseLocalDateTime("", e));
    TEST_ASSERT_FALSE(parseLocalDateTime("gestern", e));
    TEST_ASSERT_FALSE(parseLocalDateTime("15.10.2025", e));
    TEST_ASSERT_FALSE(parseLocalDateTime("15.10.2025 14:06x", e));
    TEST_ASSERT_FALSE(parseLocalDateTime("31.04.2026 10:00", e));
    TEST_ASSERT_FALSE(parseLocalDateTime("15.13.2025 10:00", e));
    TEST_ASSERT_FALSE(parseLocalDateTime("15.10.2025 24:00", e));
    TEST_ASSERT_FALSE(parseLocalDateTime("2025-10-15 14:06", e));
    TEST_ASSERT_EQUAL_INT64(7, e);
}

void test_trim_and_utf8_length()
{
    TEST_ASSERT_EQUAL_STRING("Test Kunde", trim(" \tTest Kunde\r\n").c_str());
    TEST_ASSERT_EQUAL_STRING("", trim(" \t ").c_str());
    TEST_ASSERT_EQUAL_UINT(6, utf8Length("Müller"));
    TEST_ASSERT_EQUAL_UINT(0, utf8Length(""));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_chf);
    RUN_TEST(test_zurich_offset_at_dst_boundaries);
    RUN_TEST(test_date_time_local);
    RUN_TEST(test_date_time_across_dst_switch);
    RUN_TEST(test_duration);
    RUN_TEST(test_parse_local_date_time);
    RUN_TEST(test_parse_local_date_time_at_dst_switch);
    RUN_TEST(test_parse_local_date_time_rejects);
    RUN_TEST(test_trim_and_utf8_length);
    return UNITY_END();
}
