#pragma once

#include <stdint.h>
#include <string>
#include <vector>

#include "auth.h"
#include "barrier.h"
#include "event_log.h"
#include "gate.h"
#include "html.h"
#include "registration.h"

// Operator view on Pico 2 (/betrieb, user «betrieb»): lanes, remote opening, event log, test-data reset.

// Reasons offered for a remote opening on both lanes (US-08 AK3).
const std::vector<std::string>& remoteOpenReasons();
// Extra reason at the entry: registers the plate with the given name, then opens.
extern const char* const NEW_CUSTOMER_REASON; // «Neuer Kunde registrieren»

// Payment status of the plate from Pico 1: «kein Parkvorgang», «bezahlt», «unbezahlt (CHF 6.00)»,
// «Kasse nicht erreichbar»; "–" without a valid plate.
std::string laneStatus(KasseLink& kasse, const std::string& plate);

struct RemoteOpenResult
{
    bool open = false;       // open the barrier
    std::string plate;       // normalized plate the barrier was opened for
    Notice notice;
    bool logged = false;     // an event was added: save the log
    bool registered = false; // a new customer was registered: save the registrations
};

// «Schranke öffnen». Needs a reason from the list («Bitte Grund auswählen») and a valid plate.
// Opens only when Pico 1 confirms the session outcome (fail-safe): entry opens a session (or one is already
// open, US-08 AK4); exit closes it, unpaid -> «ohne Zahlung ausgefahren», or Pico 1 has none.
// Pico 1 not answering: stays closed, logged as «Kasse nicht erreichbar»; other replies: stays closed.
// NEW_CUSTOMER_REASON (entry only): registers plate + name first; a rejected registration stays closed.
// The registration is kept even if Pico 1 then doesn't confirm.
RemoteOpenResult remoteOpen(GateLane lane, const std::string& rawPlate, const std::string& reason,
                            const std::string& name, int64_t now, KasseLink& kasse, RegistrationList& registrations,
                            EventLog& log);

// Logs «Kasse nicht erreichbar» if the gate result says so. True if an event was added.
bool logGateResult(EventLog& log, GateLane lane, const GateResult& result, int64_t now);
// Logs a safety stop or loop fault. True if an event was added.
bool logBarrierEvent(EventLog& log, GateLane lane, BarrierEvent event, int64_t now);

struct OperatorLane
{
    GateLane lane = GateLane::Entry;
    const char* barrier = "geschlossen"; // barrierStateLabel()
    std::string lastPlate;               // last plate typed at this lane's terminal
    std::string status;                  // laneStatus()
    std::string fault;                   // loop fault message; empty if none
    std::string call;                    // open call (callLabel); empty if none
};

// filter: event type name (see eventTypeName) or empty for all.
std::string renderOperatorPage(const OperatorLane& entry, const OperatorLane& exit, const EventLog& log,
                               const std::string& filter, const Notice& notice);
