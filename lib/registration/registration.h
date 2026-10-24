#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

constexpr size_t MAX_REGISTRATIONS = 50;
constexpr size_t MAX_NAME_CHARS = 40; // also the maxlength of the name fields

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
    // Name is trimmed and must be 1..MAX_NAME_CHARS characters; plate is normalized, validated and unique.
    RegistrationResult add(const std::string& name, const std::string& rawPlate);
    bool remove(const std::string& rawPlate);
    bool contains(const std::string& rawPlate) const;
    const std::vector<Registration>& all() const;
    void clear();
    // Changes with every successful add/remove and with clear: compare before and after to know whether to save.
    uint32_t revision() const;

private:
    std::vector<Registration> entries;
    uint32_t changes = 0;
};
