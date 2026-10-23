#pragma once

#include <stdint.h>
#include <string>
#include <vector>

#include "barrier.h"
#include "event_log.h"
#include "gate.h"
#include "html.h"

// Operator view on Pico 2 (/betrieb, user «betrieb»): lanes, remote opening, event log.

extern const char* const OPERATOR_USER; // "betrieb"

// Reasons offered for a remote opening (US-08 AK3).
const std::vector<std::string>& remoteOpenReasons();

// Payment status of the plate from Pico 1: «kein Parkvorgang», «bezahlt», «unbezahlt (CHF 6.00)»,
// «Kasse nicht erreichbar»; "–" without a valid plate.
std::string laneStatus(KasseLink& kasse, const std::string& plate);

struct RemoteOpenResult
{
    bool open = false;   // open the barrier
    Notice notice;
    bool logged = false; // an event was added: save the log
};

// «Schranke öffnen». Needs a reason from the list («Bitte Grund auswählen») and a valid plate.
// Opens only when Pico 1 confirms the session outcome (fail-safe): entry opens a session (or one is already
// open, US-08 AK4); exit closes it, unpaid -> «ohne Zahlung ausgefahren», or Pico 1 has none.
// Pico 1 not answering: stays closed, logged as «Kasse nicht erreichbar»; other replies: stays closed.
RemoteOpenResult remoteOpen(GateLane lane, const std::string& rawPlate, const std::string& reason, int64_t now,
                            KasseLink& kasse, EventLog& log);

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
