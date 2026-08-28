#pragma once

#include <stdint.h>

enum Phase { IDLE, RUNNING, FINISHED };

struct Countdown
{
  uint32_t durationMs;
  uint32_t remainingMs;
  uint32_t sinceFinishedMs;
  Phase    phase;
};

Countdown setDuration(Countdown c, uint32_t seconds);
Countdown start(Countdown c);
Countdown reset(Countdown c);
Countdown tick(Countdown c, uint32_t dtMs);
bool      ledOn(Countdown c);
int32_t   parseSeconds(const char* line);
