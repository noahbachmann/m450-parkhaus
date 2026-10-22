#pragma once

#include <string>

bool storageBegin();
// Reads /<name>.json; false if missing. Removes a leftover /<name>.tmp from an interrupted write.
bool storageRead(const char* name, std::string& out);
// Writes /<name>.tmp, then renames it over /<name>.json, so a power cut never leaves a half file.
bool storageWrite(const char* name, const std::string& data);
