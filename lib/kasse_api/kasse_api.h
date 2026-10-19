#pragma once

#include <stdint.h>
#include <string>

#include "session.h"

// Pico 1 HTTP API (docs/context/architecture.md, "Interface Pico 2 -> Pico 1") as pure functions.
// The web server glue only routes the request and sends status + body as application/json.

struct ApiResponse
{
    int status;
    std::string body;
};

// GET /api/status?kz=<plate>
ApiResponse handleStatus(const SessionStore& store, const std::string& kzParam, int64_t now);

// POST /api/entry, body {"kz":"..."} or {"kz":"...","entry":<epoch>}
ApiResponse handleEntry(SessionStore& store, const std::string& body, int64_t now, bool timeSynced);

// POST /api/exit, body {"kz":"..."}
ApiResponse handleExit(SessionStore& store, const std::string& body, int64_t now, bool timeSynced);
