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

// POST /api/exit, body {"kz":"..."}; {"kz":"...","override":true} = operator remote opening (closes unpaid
// sessions as closed_unpaid)
ApiResponse handleExit(SessionStore& store, const std::string& body, int64_t now, bool timeSynced);

// POST /api/pay, body {"kz":"...","method":"card_ok"|"card_declined"|"twint"} — payment at the exit terminal
// (no cash there). 200 {"kz","status":"bezahlt","amount","method","tx"} · 402 declined · 409 already_paid /
// nothing_to_pay · 404 no_session · 400 invalid_plate / bad_request · 503 time_not_synced
ApiResponse handlePay(SessionStore& store, const std::string& body, int64_t now, bool timeSynced);
