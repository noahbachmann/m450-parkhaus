#pragma once

#include <stdint.h>

// Values from docs/context/tariff.md (source of truth). Amounts in Rappen.
constexpr int64_t FREE_MINUTES = 15;
constexpr int64_t UNIT_MINUTES = 60;
constexpr uint32_t PRICE_PER_UNIT_RAPPEN = 200;
constexpr int64_t DAY_BLOCK_HOURS = 24;
constexpr uint32_t DAILY_MAX_RAPPEN = 2000;

// Times in Unix epoch seconds. Returns false if end is before entry (no fee calculated).
bool calculateFee(int64_t entryEpoch, int64_t endEpoch, uint32_t& feeRappen);
