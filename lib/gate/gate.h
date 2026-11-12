#pragma once

#include <stdint.h>
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
    bool unpaid = false;      // exit refused because of an unpaid fee (payment offered at the terminal)
    uint32_t feeRappen = 0;   // the unpaid fee
    uint32_t cashRappen = 0;  // of it already paid in cash at the Automat (the rest is due)
};

// "error" field of a Pico 1 error reply ({"error":"<code>"}); empty if there is none.
std::string errorCode(const HttpReply& reply);

// Entry: valid + registered plate -> POST /api/entry. entryTimeInput empty = now (Pico 1 stamps it),
// otherwise a backdated Europe/Zurich local time (see parseLocalDateTime).
GateResult entryRequest(const RegistrationList& registrations, const std::string& rawPlate,
                        const std::string& entryTimeInput, KasseLink& kasse);

// Exit: valid plate -> POST /api/exit; opens on 200 closed / closed_free only. Unpaid: «Gebühr CHF 6.00 nicht
// bezahlt – …», after partial cash «Restbetrag CHF 1.50 von CHF 4.00 nicht bezahlt – …».
GateResult exitRequest(const std::string& rawPlate, KasseLink& kasse);

// A plate captured while the lane's barrier is still open or closing: one opening per vehicle, so it is not
// passed on to Pico 1 («Bitte warten, bis das vorherige Fahrzeug durchgefahren ist»).
GateResult laneBusy(const std::string& rawPlate);
