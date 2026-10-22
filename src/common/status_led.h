#pragma once

void statusLedBegin();
// Call every loop(): solid when ready, blinking (2 Hz) otherwise. Never blocks.
void statusLedUpdate(bool ready, unsigned long nowMs);
