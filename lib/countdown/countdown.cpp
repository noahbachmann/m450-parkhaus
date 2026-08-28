#include "countdown.h"

Countdown setDuration(Countdown c, uint32_t seconds)
{
  (void)seconds;
  return c;
}

Countdown start(Countdown c)
{
  return c;
}

Countdown reset(Countdown c)
{
  return c;
}

Countdown tick(Countdown c, uint32_t dtMs)
{
  (void)dtMs;
  return c;
}

bool ledOn(Countdown c)
{
  (void)c;
  return false;
}

int32_t parseSeconds(const char* line)
{
  (void)line;
  return -1;
}
