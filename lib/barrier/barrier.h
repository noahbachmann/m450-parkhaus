#pragma once

#include <stdint.h>

constexpr uint32_t BARRIER_MIN_OPEN_MS = 5000;
constexpr int BARRIER_CLOSED_ANGLE = 0;
constexpr int BARRIER_OPEN_ANGLE = 90;

enum class BarrierState
{
    Closed,
    Open,
};

// One barrier. Times are millis() values; differences are wrap-safe.
class Barrier
{
public:
    // Opens (or keeps open and restarts the minimum open time).
    void open(uint32_t nowMs);
    // Call every loop(). Closes once BARRIER_MIN_OPEN_MS have passed and the loop is free.
    void update(uint32_t nowMs, bool loopOccupied);

    BarrierState state() const;
    // Servo angle for the current state.
    int angle() const;

private:
    BarrierState current = BarrierState::Closed;
    uint32_t openedAtMs = 0;
};
