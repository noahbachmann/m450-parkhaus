#include "barrier.h"

void Barrier::open(uint32_t nowMs)
{
    current = BarrierState::Open;
    openedAtMs = nowMs;
}

void Barrier::update(uint32_t nowMs, bool loopOccupied)
{
    if (current == BarrierState::Open && nowMs - openedAtMs >= BARRIER_MIN_OPEN_MS && !loopOccupied)
    {
        current = BarrierState::Closed;
    }
}

BarrierState Barrier::state() const
{
    return current;
}

int Barrier::angle() const
{
    return current == BarrierState::Open ? BARRIER_OPEN_ANGLE : BARRIER_CLOSED_ANGLE;
}
