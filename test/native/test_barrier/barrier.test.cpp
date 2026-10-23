#include <unity.h>

#include "barrier.h"

void setUp()
{
}

void tearDown()
{
}

void test_starts_closed()
{
    Barrier b;
    b.update(100000, false);
    TEST_ASSERT_EQUAL(BarrierState::Closed, b.state());
    TEST_ASSERT_EQUAL_INT(BARRIER_CLOSED_ANGLE, b.angle());
}

void test_opens_immediately_and_closes_after_min_time_without_loop()
{
    Barrier b;
    b.open(1000);
    TEST_ASSERT_EQUAL(BarrierState::Open, b.state());
    TEST_ASSERT_EQUAL_INT(BARRIER_OPEN_ANGLE, b.angle());
    b.update(1000 + BARRIER_MIN_OPEN_MS - 1, false);
    TEST_ASSERT_EQUAL(BarrierState::Open, b.state());
    b.update(1000 + BARRIER_MIN_OPEN_MS, false);
    TEST_ASSERT_EQUAL(BarrierState::Closed, b.state());
}

void test_stays_open_while_loop_occupied_tf04()
{
    Barrier b;
    b.open(0);
    b.update(2000, true);
    b.update(30000, true);
    TEST_ASSERT_EQUAL(BarrierState::Open, b.state());
    b.update(30001, false);
    TEST_ASSERT_EQUAL(BarrierState::Closed, b.state());
}

void test_reopen_restarts_min_time()
{
    Barrier b;
    b.open(0);
    b.open(4000);
    b.update(BARRIER_MIN_OPEN_MS, false);
    TEST_ASSERT_EQUAL(BarrierState::Open, b.state());
    b.update(4000 + BARRIER_MIN_OPEN_MS, false);
    TEST_ASSERT_EQUAL(BarrierState::Closed, b.state());
}

void test_millis_wraparound()
{
    Barrier b;
    b.open(0xFFFFFFFFu - 1000);
    b.update(1000, false);
    TEST_ASSERT_EQUAL(BarrierState::Open, b.state());
    b.update(BARRIER_MIN_OPEN_MS, false);
    TEST_ASSERT_EQUAL(BarrierState::Closed, b.state());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_starts_closed);
    RUN_TEST(test_opens_immediately_and_closes_after_min_time_without_loop);
    RUN_TEST(test_stays_open_while_loop_occupied_tf04);
    RUN_TEST(test_reopen_restarts_min_time);
    RUN_TEST(test_millis_wraparound);
    return UNITY_END();
}
