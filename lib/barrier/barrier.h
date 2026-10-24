#pragma once

#include <stdint.h>
#include <string>

constexpr uint32_t BARRIER_MIN_OPEN_MS = 5000;
constexpr uint32_t BARRIER_CLOSE_MS = 2000;       // closing sweep, slow enough to stop in time
constexpr uint32_t LOOP_FAULT_MS = 5 * 60 * 1000; // loop occupied this long -> fault message
constexpr uint32_t LOOP_DEBOUNCE_MS = 50;          // loop input must be stable this long (well below TF-13's 0.5 s)
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
//   Closed --open()--> Open --vehicle passed, min time over, loop free--> Closing --sweep done--> Closed
//   Closing --loop occupied--> Open (SafetyStop), min time restarts
// The vehicle has passed after vehiclePassed() («Durchfahren» on the camera page) or when the loop goes
// from occupied to free while the barrier is up.
class Barrier
{
public:
    // Start-up state. A loop occupied at boot means a car may be under the arm: start open instead of closing
    // onto it; it closes normally once the car has left.
    void begin(uint32_t nowMs, bool loopOccupied);
    // Opens (or keeps open) for a new vehicle and restarts the minimum open time.
    void open(uint32_t nowMs);
    // The vehicle drove through. False if the barrier is closed.
    bool vehiclePassed();
    // Call every loop() with the loop input; returns what happened in this call.
    BarrierEvent update(uint32_t nowMs, bool loopOccupied);

    BarrierState state() const;
    // Servo angle; moves from open to closed during Closing.
    int angle() const;
    // Loop occupied for at least LOOP_FAULT_MS right now.
    bool loopFault() const;

private:
    void reopen(uint32_t nowMs);

    BarrierState current = BarrierState::Closed;
    uint32_t openedAtMs = 0;
    uint32_t closingSinceMs = 0;
    int currentAngle = BARRIER_CLOSED_ANGLE;
    bool occupied = false;
    uint32_t occupiedSinceMs = 0;
    bool fault = false;
    bool passed = false;
};

// Core-1 side of a barrier whose commands arrive as running counters (Pico 2: written by core 0). Each new count
// is applied once, open before pass, then Barrier::update() with the loop input.
class BarrierDriver
{
public:
    void begin(uint32_t nowMs, bool loopOccupied);
    BarrierEvent update(uint32_t nowMs, uint32_t openRequests, uint32_t passRequests, bool loopOccupied);
    const Barrier& barrier() const;
    // passRequests value applied last, and whether the barrier accepted that pass (Barrier::vehiclePassed()).
    uint32_t passesHandled() const;
    bool lastPassAccepted() const;

private:
    Barrier current;
    uint32_t seenOpen = 0;
    uint32_t seenPass = 0;
    bool passAccepted = false;
};

// Loop input that counts only after it has been stable for LOOP_DEBOUNCE_MS, so a bouncing button or a noisy
// IR sensor neither stops the barrier nor counts as a passed vehicle.
class Debouncer
{
public:
    explicit Debouncer(bool initial = false);
    bool update(uint32_t nowMs, bool raw);

private:
    bool stable;
    bool last;
    uint32_t changedAtMs = 0;
};

// «offen», «schliesst», «geschlossen»
const char* barrierStateLabel(BarrierState state);

// «Sicherungsschleife Einfahrt länger als 5 min belegt»
std::string loopFaultMessage(const char* laneName);
