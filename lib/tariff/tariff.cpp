#include "tariff.h"

namespace
{
    uint32_t unitsFee(int64_t units)
    {
        int64_t fee = units * PRICE_PER_UNIT_RAPPEN;
        return fee < DAILY_MAX_RAPPEN ? static_cast<uint32_t>(fee) : DAILY_MAX_RAPPEN;
    }

    int64_t startedUnits(int64_t seconds)
    {
        return (seconds + UNIT_SECONDS - 1) / UNIT_SECONDS;
    }
}

bool feeBreakdown(int64_t durationSeconds, FeeBreakdown& out)
{
    if (durationSeconds < 0)
    {
        return false;
    }
    FeeBreakdown b;
    if (durationSeconds <= FREE_SECONDS)
    {
        b.free = true;
        out = b;
        return true;
    }
    b.fullDays = durationSeconds / DAY_BLOCK_SECONDS;
    b.dayFeeRappen = unitsFee(startedUnits(DAY_BLOCK_SECONDS));
    b.restUnits = startedUnits(durationSeconds % DAY_BLOCK_SECONDS);
    b.restCapped = b.restUnits * PRICE_PER_UNIT_RAPPEN > DAILY_MAX_RAPPEN;
    b.feeRappen = static_cast<uint32_t>(b.fullDays) * b.dayFeeRappen + unitsFee(b.restUnits);
    out = b;
    return true;
}

bool calculateFee(int64_t entryEpoch, int64_t endEpoch, uint32_t& feeRappen)
{
    FeeBreakdown b;
    if (!feeBreakdown(endEpoch - entryEpoch, b))
    {
        return false;
    }
    feeRappen = b.feeRappen;
    return true;
}
