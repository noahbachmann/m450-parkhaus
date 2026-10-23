#pragma once

#include <stdint.h>
#include <string>

constexpr uint32_t BARRIER_MIN_OPEN_MS = 5000;
constexpr uint32_t BARRIER_CLOSE_MS = 2000;       // closing sweep, slow enough to stop in time
constexpr uint32_t LOOP_FAULT_MS = 5 * 60 * 1000; // loop occupied this long -> fault message
constexpr int BARRIER_CLOSED_ANGLE = 0;
constexpr int BARRIER_OPEN_ANGLE = 90;

enum class BarrierState
{
    Closed,
    Open,
    Closing,
};

enum class BarrierEvent
{
    None,
    SafetyStop, // loop triggered while closing: stopped and reopened
    LoopFault,  // loop occupied for LOOP_FAULT_MS (reported once per occupation)
};

// One barrier with its safety loop. Times are millis() values; differences are wrap-safe.
//   Closed --open()--> Open --min time over, loop free--> Closing --sweep done--> Closed
//   Closing --loop occupied--> Open (SafetyStop), min time restarts
class Barrier
{
public:
    // Opens (or keeps open) and restarts the minimum open time.
    void open(uint32_t nowMs);
    // Call every loop() with the loop input; returns what happened in this call.
    BarrierEvent update(uint32_t nowMs, bool loopOccupied);

    BarrierState state() const;
    // Servo angle; moves from open to closed during Closing.
    int angle() const;
    // Loop occupied for at least LOOP_FAULT_MS right now.
    bool loopFault() const;

private:
    BarrierState current = BarrierState::Closed;
    uint32_t openedAtMs = 0;
    uint32_t closingSinceMs = 0;
    int currentAngle = BARRIER_CLOSED_ANGLE;
    bool occupied = false;
    uint32_t occupiedSinceMs = 0;
    bool fault = false;
};

// «offen», «schliesst», «geschlossen»
const char* barrierStateLabel(BarrierState state);

// «Sicherungsschleife Einfahrt länger als 5 min belegt»
std::string loopFaultMessage(const char* laneName);
