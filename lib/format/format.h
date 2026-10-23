#pragma once

#include <stdint.h>
#include <string>

// 400 -> "CHF 4.00"
std::string formatChf(uint32_t rappen);

// Offset of Europe/Zurich from UTC at the given instant: 3600 (CET) or 7200 (CEST).
// CEST runs from the last Sunday of March 01:00 UTC to the last Sunday of October 01:00 UTC.
int32_t zurichOffsetSeconds(int64_t epoch);

// Epoch seconds (UTC) -> "TT.MM.JJJJ HH:MM:SS" in Europe/Zurich local time.
std::string formatDateTime(int64_t epoch);

// Seconds -> "<h> h <m> min", minutes rounded down: 4800 -> "1 h 20 min", 300 -> "0 h 5 min".
// Negative durations are shown as zero.
std::string formatDuration(int64_t seconds);

// Europe/Zurich local time -> epoch seconds (UTC). Accepts "JJJJ-MM-TTTHH:MM[:SS]" (HTML datetime-local)
// and "TT.MM.JJJJ HH:MM[:SS]", surrounding whitespace ignored. In the autumn overlap hour the earlier
// (CEST) instant wins; a time in the spring gap is read as CET. False for anything else.
bool parseLocalDateTime(const std::string& text, int64_t& epoch);
