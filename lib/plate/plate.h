#pragma once

#include <string>

// Uppercase and remove all whitespace: "lu 123 456" -> "LU123456".
std::string normalizePlate(const std::string& raw);

// Expects a normalized plate. Valid = 2 letters A-Z followed by 1-6 digits.
bool isValidPlate(const std::string& normalized);
