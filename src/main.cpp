#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  static bool state = false;
  state = !state;
  digitalWrite(LED_BUILTIN, state);
  Serial.println(state ? "on" : "off");
  delay(200);
}