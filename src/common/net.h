#pragma once

#include <IPAddress.h>
#include <stdint.h>

// Starts joining the WLAN with a fixed IP. Returns immediately.
void netBegin(const IPAddress& ip);
// Call every loop(): retries the WLAN, starts NTP once connected, logs state changes.
void netUpdate(unsigned long nowMs);
bool wifiReady();
bool timeReady();
// Unix epoch seconds (UTC). Only meaningful when timeReady().
int64_t nowUtc();
