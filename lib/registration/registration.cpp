#include "registration.h"

#include <algorithm>
#include <cctype>

#include "plate.h"

namespace
{
    bool isSpace(char c)
    {
        return std::isspace(static_cast<unsigned char>(c)) != 0;
    }

    std::string trim(const std::string& s)
    {
        auto begin = std::find_if_not(s.begin(), s.end(), isSpace);
        auto end = std::find_if_not(s.rbegin(), s.rend(), isSpace).base();
        return begin < end ? std::string(begin, end) : std::string();
    }
}

RegistrationResult RegistrationList::add(const std::string& name, const std::string& rawPlate)
{
    std::string plate = normalizePlate(rawPlate);
    if (!isValidPlate(plate))
    {
        return RegistrationResult::InvalidPlate;
    }
    std::string trimmed = trim(name);
    if (trimmed.empty() || trimmed.size() > MAX_NAME_BYTES)
    {
        return RegistrationResult::InvalidName;
    }
    if (contains(plate))
    {
        return RegistrationResult::Duplicate;
    }
    if (entries.size() >= MAX_REGISTRATIONS)
    {
        return RegistrationResult::Full;
    }
    entries.push_back({trimmed, plate});
    return RegistrationResult::Ok;
}

bool RegistrationList::remove(const std::string& rawPlate)
{
    std::string plate = normalizePlate(rawPlate);
    auto it = std::find_if(entries.begin(), entries.end(), [&](const Registration& r) { return r.plate == plate; });
    if (it == entries.end())
    {
        return false;
    }
    entries.erase(it);
    return true;
}

bool RegistrationList::contains(const std::string& rawPlate) const
{
    std::string plate = normalizePlate(rawPlate);
    return std::any_of(entries.begin(), entries.end(), [&](const Registration& r) { return r.plate == plate; });
}

const std::vector<Registration>& RegistrationList::all() const
{
    return entries;
}

void RegistrationList::clear()
{
    entries.clear();
}
