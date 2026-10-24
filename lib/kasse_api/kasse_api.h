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

// GET /api/status?kz=<plate>. 503 time_not_synced if the clock is before an open session's entry.
ApiResponse handleStatus(const SessionStore& store, const std::string& kzParam, int64_t now);

// POST /api/entry, body {"kz":"..."} or {"kz":"...","entry":<epoch>}. A repeated entry (see
// SessionStore::openEntry) answers 201 with the existing session's entry time.
ApiResponse handleEntry(SessionStore& store, const std::string& body, int64_t now, bool timeSynced);

// POST /api/exit, body {"kz":"..."}; {"kz":"...","override":true} = operator remote opening (closes unpaid
// sessions as closed_unpaid)
ApiResponse handleExit(SessionStore& store, const std::string& body, int64_t now, bool timeSynced);

// POST /api/pay, body {"kz":"...","method":"card_ok"|"card_declined"|"twint"} (exit terminal, Kasse) or
// {"kz":"...","method":"cash","amount":<Rappen inserted>} (Kasse).
// 200 {"kz","status":"bezahlt","amount","method":"card"|"twint"|"cash","tx"} (+ "change" for cash, + "cash" for
// card/TWINT after partial cash: the part already inserted) ·
// 200 {"kz","status":"unbezahlt","amount":<fee>,"remaining"} after partial cash · 402 declined ·
// 409 already_paid / nothing_to_pay · 404 no_session · 400 invalid_plate / invalid_amount / bad_request ·
// 503 time_not_synced
ApiResponse handlePay(SessionStore& store, const std::string& body, int64_t now, bool timeSynced);

// GET /api/sessions — all sessions for the Kasse page and the camera page on Pico 2, in the flash file format
// (see codec.h) plus "now" (Pico 1 time, authoritative for fees) and "synced".
ApiResponse handleSessions(const SessionStore& store, int64_t now, bool timeSynced);

// POST /api/reset — test-data reset: no sessions, tx counter 1. 200 {"status":"reset"}.
ApiResponse handleReset(SessionStore& store);
