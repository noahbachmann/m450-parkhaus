#include "plate.h"

#include <cctype>

const char* const INVALID_PLATE_TEXT = "Ungültiges Kennzeichen – erwartet 2 Buchstaben und 1–6 Ziffern";

std::string normalizePlate(const std::string& raw)
{
    std::string result;
    result.reserve(raw.size());
    for (char c : raw)
    {
        unsigned char u = static_cast<unsigned char>(c);
        if (std::isspace(u))
        {
            continue;
        }
        result += static_cast<char>(std::toupper(u));
    }
    return result;
}

bool isValidPlate(const std::string& normalized)
{
    const size_t letters = 2;
    const size_t maxDigits = 6;
    if (normalized.size() < letters + 1 || normalized.size() > letters + maxDigits)
    {
        return false;
    }
    for (size_t i = 0; i < normalized.size(); ++i)
    {
        char c = normalized[i];
        bool ok = i < letters ? (c >= 'A' && c <= 'Z') : (c >= '0' && c <= '9');
        if (!ok)
        {
            return false;
        }
    }
    return true;
}
