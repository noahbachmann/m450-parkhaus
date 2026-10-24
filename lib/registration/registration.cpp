#include "registration.h"

#include <algorithm>

#include "format.h"
#include "plate.h"

RegistrationResult RegistrationList::add(const std::string& name, const std::string& rawPlate)
{
    std::string plate = normalizePlate(rawPlate);
    if (!isValidPlate(plate))
    {
        return RegistrationResult::InvalidPlate;
    }
    std::string trimmed = trim(name);
    if (trimmed.empty() || utf8Length(trimmed) > MAX_NAME_CHARS)
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
