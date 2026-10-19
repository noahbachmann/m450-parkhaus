#include "format.h"

#include <cstdio>

namespace
{
    constexpr int64_t DAY = 86400;

    int64_t floorDiv(int64_t a, int64_t b)
    {
        int64_t q = a / b;
        return (a % b != 0 && (a < 0) != (b < 0)) ? q - 1 : q;
    }

    // Howard Hinnant's days_from_civil / civil_from_days (proleptic Gregorian).
    int64_t daysFromCivil(int64_t y, unsigned m, unsigned d)
    {
        y -= m <= 2;
        const int64_t era = floorDiv(y, 400);
        const unsigned yoe = static_cast<unsigned>(y - era * 400);
        const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
        const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + static_cast<int64_t>(doe) - 719468;
    }

    struct Civil
    {
        int64_t year;
        unsigned month;
        unsigned day;
    };

    Civil civilFromDays(int64_t z)
    {
        z += 719468;
        const int64_t era = floorDiv(z, 146097);
        const unsigned doe = static_cast<unsigned>(z - era * 146097);
        const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        const unsigned mp = (5 * doy + 2) / 153;
        const unsigned d = doy - (153 * mp + 2) / 5 + 1;
        const unsigned m = mp < 10 ? mp + 3 : mp - 9;
        return {static_cast<int64_t>(yoe) + era * 400 + (m <= 2), m, d};
    }

    // 0 = Sunday. 1970-01-01 was a Thursday.
    unsigned weekday(int64_t days)
    {
        return static_cast<unsigned>((days % 7 + 7 + 4) % 7);
    }

    int64_t lastSundayOf31DayMonth(int64_t year, unsigned month)
    {
        int64_t last = daysFromCivil(year, month, 31);
        return last - weekday(last);
    }
}

std::string formatChf(uint32_t rappen)
{
    char buf[24];
    std::snprintf(buf, sizeof(buf), "CHF %lu.%02lu", static_cast<unsigned long>(rappen / 100),
                  static_cast<unsigned long>(rappen % 100));
    return buf;
}

int32_t zurichOffsetSeconds(int64_t epoch)
{
    int64_t year = civilFromDays(floorDiv(epoch, DAY)).year;
    int64_t start = lastSundayOf31DayMonth(year, 3) * DAY + 3600;
    int64_t end = lastSundayOf31DayMonth(year, 10) * DAY + 3600;
    return (epoch >= start && epoch < end) ? 7200 : 3600;
}

std::string formatDateTime(int64_t epoch)
{
    int64_t local = epoch + zurichOffsetSeconds(epoch);
    int64_t days = floorDiv(local, DAY);
    int64_t secs = local - days * DAY;
    Civil c = civilFromDays(days);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%02u.%02u.%04ld %02ld:%02ld:%02ld", c.day, c.month, static_cast<long>(c.year),
                  static_cast<long>(secs / 3600), static_cast<long>(secs % 3600 / 60), static_cast<long>(secs % 60));
    return buf;
}

std::string formatDuration(int64_t seconds)
{
    if (seconds < 0)
    {
        seconds = 0;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%ld h %ld min", static_cast<long>(seconds / 3600),
                  static_cast<long>(seconds % 3600 / 60));
    return buf;
}
