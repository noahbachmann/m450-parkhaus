#pragma once

#include <stdint.h>
#include <string>

#include "html.h"
#include "payment.h"
#include "session.h"

// Texts shown on the Kasse and Automat pages (German, verbatim from test-concept.md where given).

struct SessionView
{
    std::string plate;
    std::string entry;         // "TT.MM.JJJJ HH:MM:SS"
    std::string exit;          // empty while the session is active
    std::string duration;      // "1 h 20 min", up to now while active, up to the exit once closed
    std::string fee;           // "CHF 4.00"; empty if the clock is before the entry time
    std::string status;        // «offen», «bezahlt», «abgeschlossen», «abgeschlossen (Freizeit)»
    std::string paymentStatus; // «bezahlt» / «unbezahlt»; empty once closed
    std::string payment;       // "Karte, TX-000001"; empty if not paid
    std::string cashInserted;  // "CHF 1.00" while partial cash is held; empty otherwise
    std::string tariffNote;    // free minutes / started hours; empty once closed
    bool payable = false;      // open with a fee > 0
};

SessionView describeSession(const ParkingSession& session, int64_t now);

// "2 angefangene Stunden à CHF 2.00 ab Einfahrt (15 Freiminuten überschritten)".
std::string tariffNote(int64_t durationSeconds, uint32_t feeRappen);

// The valid tariff in one sentence (tariff.md, German wording): «15 Freiminuten; wird die Freizeit
// überschritten, kostet jede angefangene Stunde ab Einfahrt CHF 2.00; Tagesmaximum CHF 20.00 pro angefangene
// 24 Stunden.»
std::string tariffSummary();

// Feedback after a payment attempt, e.g. «Bezahlt – CHF 4.00, Karte – Transaktion TX-000001».
std::string paymentMessage(const PaymentResult& result, const std::string& plate);
// True if the result is a successful booking or progress (paid, partial cash).
bool paymentSucceeded(const PaymentResult& result);

// «Kein offener Parkvorgang für SZ555»
std::string noSessionMessage(const std::string& plate);

using PayFormResult = FormResult;

// Form fields of POST /kasse/pay and /automat/pay: kz, method (card_ok|card_declined|twint|cash),
// amount in CHF (cash only).
PayFormResult handlePayForm(SessionStore& store, const std::string& kz, const std::string& method,
                            const std::string& amount, int64_t now, bool timeOk);

// Form value -> method: "card_ok", "card_declined", "twint", "cash". False for anything else.
bool parsePayMethod(const std::string& name, PayMethod& method);

// "10", "10.5", "10.50", "10,50", "CHF 10.00" -> 1050 Rappen. False for anything else, 0 or > CHF 1000.
bool parseChf(const std::string& text, uint32_t& rappen);
