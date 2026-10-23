#pragma once

#include <string>

#include "gate.h"

// Blocking HTTP/1.0 POST with Content-Type application/json, for the simulated Pico 2.
// baseUrl like "http://kasse:8081". Whole request incl. connect is bounded by timeoutMs.
// Any failure (bad URL, DNS, refused, timeout, garbage) -> status 0, like an unreachable Pico 1.
HttpReply httpPostJson(const std::string& baseUrl, const std::string& path, const std::string& body, int timeoutMs);
