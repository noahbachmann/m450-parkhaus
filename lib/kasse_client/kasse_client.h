#pragma once

#include <stdint.h>
#include <string>

#include "gate.h"
#include "html.h"
#include "session.h"

// Pico 2 side of the Kasse: the Kasse page and the camera page's vehicle list are served by Pico 2, the
// sessions stay on Pico 1 and are read / paid through its JSON API.

struct KasseSnapshot
{
    bool reachable = false; // Pico 1 answered with a valid session list
    bool synced = false;    // Pico 1 clock synced (fees and payments possible)
    int64_t now = 0;        // Pico 1 time, used for durations and fees
    SessionStore sessions;
};

// GET /api/sessions.
KasseSnapshot fetchSessions(KasseLink& kasse);

// POST /kasse/pay form fields: kz, method (card_ok|card_declined|twint|cash), amount in CHF (cash only)
// -> POST /api/pay. The notice is the Kasse text, e.g. «Bezahlt – CHF 6.00, Bargeld – Transaktion TX-000001 –
// Rückgeld CHF 4.00»; changed = a payment was booked (also partial cash).
FormResult payAtKasse(KasseLink& kasse, const std::string& kz, const std::string& method, const std::string& amount);

// POST /api/reset. False if Pico 1 didn't confirm.
bool resetKasse(KasseLink& kasse);
