#pragma once

#include <stddef.h>
#include <string>
#include <vector>

constexpr size_t MAX_REGISTRATIONS = 50;
constexpr size_t MAX_NAME_BYTES = 40;

struct Registration
{
    std::string name;
    std::string plate;
};

enum class RegistrationResult
{
    Ok,
    InvalidPlate,
    InvalidName,
    Duplicate,
    Full,
};

class RegistrationList
{
public:
    // Name is trimmed and must be 1..MAX_NAME_BYTES bytes; plate is normalized, validated and unique.
    RegistrationResult add(const std::string& name, const std::string& rawPlate);
    bool remove(const std::string& rawPlate);
    bool contains(const std::string& rawPlate) const;
    const std::vector<Registration>& all() const;
    void clear();

private:
    std::vector<Registration> entries;
};
