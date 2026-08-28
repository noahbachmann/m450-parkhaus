// Hardware smoke test - runs on the Pico only (pio test -e pico).
#include <Arduino.h>
#include <unity.h>

// unity setup
void setUp(void) {}
void tearDown(void) {}

void test_led_blinks(void)
{
  pinMode(LED_BUILTIN, OUTPUT);

  for (int i = 0; i < 3; ++i)
  {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
  }

  TEST_PASS();
}

void setup()
{
  delay(2000); // give the host time to open the serial port
  UNITY_BEGIN();
  RUN_TEST(test_led_blinks);
  UNITY_END();
}

void loop() {}
