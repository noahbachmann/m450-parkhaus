#pragma once

#include <string>

#include "registration.h"

// Entry/exit decisions on Pico 2. The HTTP call to Pico 1 goes through KasseLink, so the firmware uses
// HTTPClient and the native tests use a fake or the real kasse_api handlers.

struct HttpReply
{
    int status = 0; // HTTP status; <= 0 = no answer (timeout, connection error)
    std::string body;
};

class KasseLink
{
public:
    virtual ~KasseLink() = default;
    // POST with Content-Type application/json to Pico 1, e.g. path "/api/entry".
    virtual HttpReply postJson(const char* path, const std::string& body) = 0;
    // GET from Pico 1, e.g. "/api/status?kz=LU123456".
    virtual HttpReply getJson(const std::string& pathAndQuery) = 0;
};

struct GateResult
{
    bool open = false;    // only true on a confirmed answer from Pico 1: fail-safe
    const char* cls = "err"; // "ok" or "err", for the page
    std::string plate;    // normalized input
    std::string message;  // German terminal text
    bool unreachable = false; // Pico 1 didn't answer (logged as a fault)
};

// Entry: valid + registered plate -> POST /api/entry. entryTimeInput empty = now (Pico 1 stamps it),
// otherwise a backdated Europe/Zurich local time (see parseLocalDateTime).
GateResult entryRequest(const RegistrationList& registrations, const std::string& rawPlate,
                        const std::string& entryTimeInput, KasseLink& kasse);

// Exit: valid plate -> POST /api/exit; opens on 200 closed / closed_free only.
GateResult exitRequest(const std::string& rawPlate, KasseLink& kasse);
