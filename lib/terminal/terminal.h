#pragma once

#include <stdint.h>
#include <string>

#include "event_log.h"
#include "gate.h"

// Terminal state of one lane on Pico 2: call button, payment at the exit after an unpaid exit attempt
// (US-16 AK2-4), abort when the car turns around (US-13 AK4). Held in RAM only.

constexpr int MAX_DECLINES = 5; // then the control room is notified (US-16 AK4)

struct LaneTerminal
{
    GateLane lane = GateLane::Entry;
    std::string lastPlate;  // last plate typed at this lane
    std::string payPlate;   // exit: plate with an unpaid fee, payment offered; empty = none
    uint32_t payFeeRappen = 0;
    int declines = 0;       // declined terminal payments for payPlate in a row
    bool callPending = false; // call button pressed or payment alert, not yet handled by the operator
    int64_t callTime = 0;
    std::string callText;   // «Ruftaste gedrückt», «Zahlung 5× abgelehnt»
    bool passing = false;   // «Durchfahren» pressed, barrier not closed yet
};

// After an entry/exit decision: remembers the plate, offers payment after an unpaid exit, clears the call and
// payment state when the barrier opens.
void recordGate(LaneTerminal& t, const GateResult& result);

// Operator opened the barrier for plate: call handled, payment offer gone.
void recordRemoteOpening(LaneTerminal& t, const std::string& plate);

// «Durchfahren»: the vehicle passed the open barrier (stands in for the safety loop). barrierUp = the
// barrier accepted it (Barrier::vehiclePassed()).
GateResult driveThrough(LaneTerminal& t, bool barrierUp);
// «Fahrzeug fährt weiter» after «Fahrzeug bleibt stehen»: freeing the loop counts as the pass, so an open
// barrier gets the «Durchfahren» message unless that was already pressed; otherwise an empty message.
GateResult leftLoop(LaneTerminal& t, bool barrierUp);
// Call when the barrier is closed: after «Durchfahren» returns «… durchgefahren – Schranke geschlossen»
// once; otherwise an empty message (nothing to show).
GateResult closedAfterPass(LaneTerminal& t);

// «Ruftaste»: logs the call (lane, last plate) and shows it in the operator view.
GateResult pressCallButton(LaneTerminal& t, int64_t now, EventLog& log);

// «Fahrzeug wendet»: the exit attempt is cancelled; the session on Pico 1 is untouched and stays open.
GateResult abortAtTerminal(LaneTerminal& t);

// Payment at the exit terminal for payPlate: method "card_ok", "card_declined" or "twint" via POST /api/pay.
// Success (or nothing / already paid) -> exit is retried (POST /api/exit) and opens on success.
// The 5th decline in a row logs a payment alert and notifies the operator.
GateResult payAtTerminal(LaneTerminal& t, const std::string& method, int64_t now, KasseLink& kasse, EventLog& log);

// Text for the operator view, e.g. «Ruftaste gedrückt – BE4711 (23.10.2026 14:06:40)»; empty if no call.
std::string callLabel(const LaneTerminal& t);
