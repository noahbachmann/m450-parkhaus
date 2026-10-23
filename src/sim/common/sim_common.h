#pragma once

#include <stdint.h>
#include <string>

#include "http_server.h"

// Shared glue of the two simulator programs (Kasse, Schranken).

constexpr uint16_t KASSE_PORT = 8081;
constexpr uint16_t SCHRANKEN_PORT = 8082;

// Environment variable or fallback.
std::string envOr(const char* name, const char* fallback);
// SIM_HOST: listen address, default 127.0.0.1 (Docker: 0.0.0.0).
std::string listenHost();

int64_t nowUtc();
uint32_t nowMs();

// Files in SIM_DATA (default "sim-data"), same JSON as LittleFS on the Picos.
void makeDataDir();
bool readDataFile(const char* name, std::string& out);
void writeDataFile(const char* name, const std::string& data);
std::string dataDir();

HttpResponse redirect(const std::string& location);
HttpResponse notFound();
// HTML page with the simulator link bar inserted after <body>.
HttpResponse simPage(const std::string& html);
