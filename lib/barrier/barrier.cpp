#include "barrier.h"

void Barrier::open(uint32_t nowMs)
{
    reopen(nowMs);
    passed = false;
}

void Barrier::reopen(uint32_t nowMs)
{
    current = BarrierState::Open;
    openedAtMs = nowMs;
    currentAngle = BARRIER_OPEN_ANGLE;
}

bool Barrier::vehiclePassed()
{
    if (current == BarrierState::Closed)
    {
        return false;
    }
    passed = true;
    return true;
}

BarrierEvent Barrier::update(uint32_t nowMs, bool loopOccupied)
{
    BarrierEvent event = BarrierEvent::None;

    if (loopOccupied && !occupied)
    {
        occupiedSinceMs = nowMs;
    }
    if (!loopOccupied && occupied && current != BarrierState::Closed)
    {
        passed = true;
    }
    occupied = loopOccupied;
    if (!occupied)
    {
        fault = false;
    }
    else if (!fault && nowMs - occupiedSinceMs >= LOOP_FAULT_MS)
    {
        fault = true;
        event = BarrierEvent::LoopFault;
    }

    switch (current)
    {
    case BarrierState::Closed:
        break;
    case BarrierState::Open:
        if (passed && !occupied && nowMs - openedAtMs >= BARRIER_MIN_OPEN_MS)
        {
            current = BarrierState::Closing;
            closingSinceMs = nowMs;
        }
        break;
    case BarrierState::Closing:
        if (occupied)
        {
            reopen(nowMs);
            return BarrierEvent::SafetyStop;
        }
        uint32_t elapsed = nowMs - closingSinceMs;
        if (elapsed >= BARRIER_CLOSE_MS)
        {
            current = BarrierState::Closed;
            currentAngle = BARRIER_CLOSED_ANGLE;
        }
        else
        {
            currentAngle = BARRIER_OPEN_ANGLE -
                           static_cast<int>(static_cast<int64_t>(BARRIER_OPEN_ANGLE - BARRIER_CLOSED_ANGLE) * elapsed /
                                            BARRIER_CLOSE_MS);
        }
        break;
    }
    return event;
}

BarrierState Barrier::state() const
{
    return current;
}

int Barrier::angle() const
{
    return currentAngle;
}

bool Barrier::loopFault() const
{
    return fault;
}

const char* barrierStateLabel(BarrierState state)
{
    switch (state)
    {
    case BarrierState::Open:
        return "offen";
    case BarrierState::Closing:
        return "schliesst";
    case BarrierState::Closed:
        break;
    }
    return "geschlossen";
}

std::string loopFaultMessage(const char* laneName)
{
    return std::string("Sicherungsschleife ") + laneName + " länger als " + std::to_string(LOOP_FAULT_MS / 60000) +
           " min belegt";
}
