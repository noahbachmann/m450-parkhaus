#pragma once

#include <stdint.h>

// Pico 2 "Schranken". Keep in sync with the pin table in docs/context/architecture.md.
constexpr uint8_t PIN_ENTRY_SERVO = 8;  // phys. pin 11
constexpr uint8_t PIN_EXIT_SERVO = 9;   // phys. pin 12
constexpr uint8_t PIN_ENTRY_LOOP = 14;  // phys. pin 19, INPUT_PULLUP, LOW = occupied; optional, unconnected = free
constexpr uint8_t PIN_EXIT_LOOP = 15;   // phys. pin 20, INPUT_PULLUP, LOW = occupied; optional, unconnected = free
