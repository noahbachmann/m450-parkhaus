#include <unity.h>
#include <countdown.h>

// unity setup
void setUp(void) {}
void tearDown(void) {}

// Test state literals: {durationMs, remainingMs, sinceFinishedMs, phase}
static const Countdown IDLE_30 = {30000, 30000, 0, IDLE};
static const Countdown RUNNING_30 = {30000, 30000, 0, RUNNING};
static const Countdown FINISHED_30 = {30000, 0, 0, FINISHED};

// US1 - set a countdown, to time different durations
void test_set_duration(void)
{
  Countdown c = setDuration(Countdown{}, 30);
  TEST_ASSERT_EQUAL_UINT32(30000, c.durationMs);
  TEST_ASSERT_EQUAL_UINT32(30000, c.remainingMs);
  TEST_ASSERT_EQUAL(IDLE, c.phase);

  TEST_ASSERT_EQUAL_UINT32(30000, setDuration(IDLE_30, 0).remainingMs);
  TEST_ASSERT_EQUAL_UINT32(30000, setDuration(IDLE_30, 99999).remainingMs);
}

// US1 - the duration arrives as "set 30" over the serial console
void test_parse_seconds(void)
{
  TEST_ASSERT_EQUAL_INT32(30, parseSeconds("set 30"));
  TEST_ASSERT_EQUAL_INT32(-1, parseSeconds("set abc"));
  TEST_ASSERT_EQUAL_INT32(-1, parseSeconds(""));
}

// US2 - see whether the countdown is running or has expired
void test_led_shows_the_phase(void)
{
  TEST_ASSERT_FALSE(ledOn(IDLE_30));
  TEST_ASSERT_TRUE(ledOn(RUNNING_30));
  TEST_ASSERT_EQUAL(RUNNING, start(IDLE_30).phase);
  TEST_ASSERT_EQUAL_UINT32(30000, start(IDLE_30).remainingMs);
}

// US3 - reset the timer, to start the countdown over
void test_reset(void)
{
  Countdown running = tick(RUNNING_30, 12000);
  TEST_ASSERT_EQUAL_UINT32(18000, running.remainingMs);

  TEST_ASSERT_EQUAL(IDLE, reset(running).phase);
  TEST_ASSERT_EQUAL_UINT32(30000, reset(running).remainingMs);
  TEST_ASSERT_EQUAL_UINT32(30000, reset(running).durationMs);
  TEST_ASSERT_FALSE(ledOn(reset(FINISHED_30)));
}

// US4 - the countdown runs out
void test_countdown_expires(void)
{
  TEST_ASSERT_EQUAL_UINT32(29600, tick(RUNNING_30, 400).remainingMs);
  TEST_ASSERT_EQUAL(RUNNING, tick(RUNNING_30, 400).phase);
  TEST_ASSERT_EQUAL(FINISHED, tick(RUNNING_30, 30000).phase);
  TEST_ASSERT_EQUAL_UINT32(0, tick(RUNNING_30, 99999).remainingMs);
  TEST_ASSERT_EQUAL_UINT32(30000, tick(IDLE_30, 5000).remainingMs);
}

// US4 - see the light blink once the countdown has finished
void test_led_blinks_when_finished(void)
{
  TEST_ASSERT_TRUE(ledOn(FINISHED_30));
  TEST_ASSERT_FALSE(ledOn(tick(FINISHED_30, 500)));
  TEST_ASSERT_TRUE(ledOn(tick(FINISHED_30, 1000)));
  TEST_ASSERT_TRUE(ledOn(tick(FINISHED_30, 60000)));
}

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_set_duration);
  RUN_TEST(test_parse_seconds);
  RUN_TEST(test_led_shows_the_phase);
  RUN_TEST(test_reset);
  RUN_TEST(test_countdown_expires);
  RUN_TEST(test_led_blinks_when_finished);
  return UNITY_END();
}
