#pragma once

#include <stdint.h>

// Values from docs/context/tariff.md (source of truth). Amounts in Rappen.
constexpr int64_t FREE_MINUTES = 15;
constexpr int64_t UNIT_MINUTES = 60;
constexpr uint32_t PRICE_PER_UNIT_RAPPEN = 200;
constexpr int64_t DAY_BLOCK_HOURS = 24;
constexpr uint32_t DAILY_MAX_RAPPEN = 2000;

constexpr int64_t FREE_SECONDS = FREE_MINUTES * 60;
constexpr int64_t UNIT_SECONDS = UNIT_MINUTES * 60;
constexpr int64_t DAY_BLOCK_SECONDS = DAY_BLOCK_HOURS * 3600;

// How a fee is made up: complete day blocks, then the started units of the last, incomplete block.
struct FeeBreakdown
{
    bool free = false;          // within the free minutes, fee 0
    int64_t fullDays = 0;       // complete day blocks
    uint32_t dayFeeRappen = 0;  // fee of one complete day block
    int64_t restUnits = 0;      // started units in the incomplete last block (0 if none)
    bool restCapped = false;    // the last block hit DAILY_MAX_RAPPEN
    uint32_t feeRappen = 0;     // total
};

// Returns false for a negative duration (entry in the future): nothing is calculated.
bool feeBreakdown(int64_t durationSeconds, FeeBreakdown& out);

// Times in Unix epoch seconds. Returns false if end is before entry (no fee calculated).
bool calculateFee(int64_t entryEpoch, int64_t endEpoch, uint32_t& feeRappen);
