#include "tariff.h"

namespace
{
    constexpr int64_t FREE_SECONDS = FREE_MINUTES * 60;
    constexpr int64_t UNIT_SECONDS = UNIT_MINUTES * 60;
    constexpr int64_t DAY_BLOCK_SECONDS = DAY_BLOCK_HOURS * 3600;

    uint32_t blockFee(int64_t blockSeconds)
    {
        int64_t units = (blockSeconds + UNIT_SECONDS - 1) / UNIT_SECONDS;
        int64_t fee = units * PRICE_PER_UNIT_RAPPEN;
        return fee < DAILY_MAX_RAPPEN ? static_cast<uint32_t>(fee) : DAILY_MAX_RAPPEN;
    }
}

bool calculateFee(int64_t entryEpoch, int64_t endEpoch, uint32_t& feeRappen)
{
    int64_t duration = endEpoch - entryEpoch;
    if (duration < 0)
    {
        return false;
    }
    if (duration <= FREE_SECONDS)
    {
        feeRappen = 0;
        return true;
    }
    int64_t fullBlocks = duration / DAY_BLOCK_SECONDS;
    int64_t rest = duration % DAY_BLOCK_SECONDS;
    uint32_t fee = static_cast<uint32_t>(fullBlocks) * blockFee(DAY_BLOCK_SECONDS);
    if (rest > 0)
    {
        fee += blockFee(rest);
    }
    feeRappen = fee;
    return true;
}
