#include <unity.h>

#include "barrier.h"

namespace
{
    // Calls update() every 10 ms like loop() would; returns the last non-None event.
    BarrierEvent run(Barrier& b, uint32_t& nowMs, uint32_t durationMs, bool loopOccupied)
    {
        BarrierEvent last = BarrierEvent::None;
        for (uint32_t end = nowMs + durationMs; nowMs != end;)
        {
            nowMs += 10;
            BarrierEvent e = b.update(nowMs, loopOccupied);
            if (e != BarrierEvent::None)
            {
                last = e;
            }
        }
        return last;
    }
}

void setUp()
{
}

void tearDown()
{
}

void test_starts_closed()
{
    Barrier b;
    TEST_ASSERT_EQUAL(BarrierEvent::None, b.update(100000, false));
    TEST_ASSERT_EQUAL(BarrierState::Closed, b.state());
    TEST_ASSERT_EQUAL_INT(BARRIER_CLOSED_ANGLE, b.angle());
}

void test_opens_immediately_then_sweeps_closed()
{
    Barrier b;
    b.open(1000);
    TEST_ASSERT_EQUAL(BarrierState::Open, b.state());
    TEST_ASSERT_EQUAL_INT(BARRIER_OPEN_ANGLE, b.angle());
    b.update(1000 + BARRIER_MIN_OPEN_MS - 1, false);
    TEST_ASSERT_EQUAL(BarrierState::Open, b.state());
    b.update(1000 + BARRIER_MIN_OPEN_MS, false);
    TEST_ASSERT_EQUAL(BarrierState::Closing, b.state());
    b.update(1000 + BARRIER_MIN_OPEN_MS + BARRIER_CLOSE_MS / 2, false);
    TEST_ASSERT_EQUAL_INT((BARRIER_OPEN_ANGLE + BARRIER_CLOSED_ANGLE) / 2, b.angle());
    b.update(1000 + BARRIER_MIN_OPEN_MS + BARRIER_CLOSE_MS, false);
    TEST_ASSERT_EQUAL(BarrierState::Closed, b.state());
    TEST_ASSERT_EQUAL_INT(BARRIER_CLOSED_ANGLE, b.angle());
}

void test_tf12_stays_open_while_occupied()
{
    Barrier b;
    uint32_t now = 0;
    b.open(now);
    run(b, now, 2000, false);
    run(b, now, 30000, true);
    TEST_ASSERT_EQUAL(BarrierState::Open, b.state());
    TEST_ASSERT_EQUAL_INT(BARRIER_OPEN_ANGLE, b.angle());
    run(b, now, 10, false);
    TEST_ASSERT_EQUAL(BarrierState::Closing, b.state());
    run(b, now, BARRIER_CLOSE_MS, false);
    TEST_ASSERT_EQUAL(BarrierState::Closed, b.state());
}

void test_tf13_loop_during_closing_stops_and_reopens()
{
    Barrier b;
    uint32_t now = 0;
    for (int round = 0; round < 3; ++round)
    {
        b.open(now);
        run(b, now, BARRIER_MIN_OPEN_MS + 1000, false); // 1 s into the closing sweep
        TEST_ASSERT_EQUAL(BarrierState::Closing, b.state());
        TEST_ASSERT_LESS_THAN_INT(BARRIER_OPEN_ANGLE, b.angle());
        now += 10;
        TEST_ASSERT_EQUAL(BarrierEvent::SafetyStop, b.update(now, true)); // reacts in the same call
        TEST_ASSERT_EQUAL(BarrierState::Open, b.state());
        TEST_ASSERT_EQUAL_INT(BARRIER_OPEN_ANGLE, b.angle());
        run(b, now, 3000, true);
        TEST_ASSERT_EQUAL(BarrierState::Open, b.state());
    }
    run(b, now, BARRIER_MIN_OPEN_MS + BARRIER_CLOSE_MS + 10, false);
    TEST_ASSERT_EQUAL(BarrierState::Closed, b.state());
}

void test_no_safety_stop_when_closed_or_open()
{
    Barrier b;
    uint32_t now = 0;
    TEST_ASSERT_EQUAL(BarrierEvent::None, run(b, now, 1000, true));
    TEST_ASSERT_EQUAL(BarrierState::Closed, b.state());
    b.open(now);
    TEST_ASSERT_EQUAL(BarrierEvent::None, run(b, now, 1000, true));
}

void test_tf15_fault_after_five_minutes_once()
{
    Barrier b;
    uint32_t now = 0;
    b.open(now);
    TEST_ASSERT_EQUAL(BarrierEvent::None, run(b, now, 4 * 60000 + 50000, true)); // 4:50
    TEST_ASSERT_FALSE(b.loopFault());
    TEST_ASSERT_EQUAL(BarrierEvent::LoopFault, run(b, now, 20000, true)); // 5:10
    TEST_ASSERT_TRUE(b.loopFault());
    TEST_ASSERT_EQUAL(BarrierState::Open, b.state());
    TEST_ASSERT_EQUAL(BarrierEvent::None, run(b, now, 60000, true)); // reported once
    run(b, now, 10, false);
    TEST_ASSERT_FALSE(b.loopFault());
}

void test_fault_timer_restarts_per_occupation()
{
    Barrier b;
    uint32_t now = 0;
    run(b, now, 4 * 60000, true);
    run(b, now, 10, false);
    TEST_ASSERT_EQUAL(BarrierEvent::None, run(b, now, 4 * 60000, true));
    TEST_ASSERT_FALSE(b.loopFault());
}

void test_reopen_restarts_min_time()
{
    Barrier b;
    b.open(0);
    b.open(4000);
    b.update(BARRIER_MIN_OPEN_MS, false);
    TEST_ASSERT_EQUAL(BarrierState::Open, b.state());
    b.update(4000 + BARRIER_MIN_OPEN_MS, false);
    TEST_ASSERT_EQUAL(BarrierState::Closing, b.state());
    b.open(4000 + BARRIER_MIN_OPEN_MS + 500); // new entry while closing
    TEST_ASSERT_EQUAL(BarrierState::Open, b.state());
    TEST_ASSERT_EQUAL_INT(BARRIER_OPEN_ANGLE, b.angle());
}

void test_millis_wraparound()
{
    Barrier b;
    b.open(0xFFFFFFFFu - 1000);
    b.update(1000, false);
    TEST_ASSERT_EQUAL(BarrierState::Open, b.state());
    b.update(BARRIER_MIN_OPEN_MS, false);
    TEST_ASSERT_EQUAL(BarrierState::Closing, b.state());
}

void test_labels()
{
    TEST_ASSERT_EQUAL_STRING("offen", barrierStateLabel(BarrierState::Open));
    TEST_ASSERT_EQUAL_STRING("schliesst", barrierStateLabel(BarrierState::Closing));
    TEST_ASSERT_EQUAL_STRING("geschlossen", barrierStateLabel(BarrierState::Closed));
    TEST_ASSERT_EQUAL_STRING("Sicherungsschleife Einfahrt länger als 5 min belegt", loopFaultMessage("Einfahrt").c_str());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_starts_closed);
    RUN_TEST(test_opens_immediately_then_sweeps_closed);
    RUN_TEST(test_tf12_stays_open_while_occupied);
    RUN_TEST(test_tf13_loop_during_closing_stops_and_reopens);
    RUN_TEST(test_no_safety_stop_when_closed_or_open);
    RUN_TEST(test_tf15_fault_after_five_minutes_once);
    RUN_TEST(test_fault_timer_restarts_per_occupation);
    RUN_TEST(test_reopen_restarts_min_time);
    RUN_TEST(test_millis_wraparound);
    RUN_TEST(test_labels);
    return UNITY_END();
}
