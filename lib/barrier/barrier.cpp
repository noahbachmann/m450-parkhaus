#include "barrier.h"

namespace
{
    // Linear from open to closed over BARRIER_CLOSE_MS.
    int sweepAngle(uint32_t elapsedMs)
    {
        int64_t range = BARRIER_OPEN_ANGLE - BARRIER_CLOSED_ANGLE;
        return BARRIER_OPEN_ANGLE - static_cast<int>(range * elapsedMs / BARRIER_CLOSE_MS);
    }
}

void Barrier::begin(uint32_t nowMs, bool loopOccupied)
{
    *this = Barrier();
    occupied = loopOccupied;
    occupiedSinceMs = nowMs;
    if (loopOccupied)
    {
        reopen(nowMs);
    }
}

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
    BarrierEvent loopEvent = trackLoop(nowMs, loopOccupied);
    BarrierEvent stateEvent = stepStateMachine(nowMs);
    return stateEvent != BarrierEvent::None ? stateEvent : loopEvent;
}

BarrierEvent Barrier::trackLoop(uint32_t nowMs, bool loopOccupied)
{
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
        return BarrierEvent::None;
    }
    if (!fault && nowMs - occupiedSinceMs >= LOOP_FAULT_MS)
    {
        fault = true;
        return BarrierEvent::LoopFault;
    }
    return BarrierEvent::None;
}

BarrierEvent Barrier::stepStateMachine(uint32_t nowMs)
{
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
        return stepClosing(nowMs);
    }
    return BarrierEvent::None;
}

BarrierEvent Barrier::stepClosing(uint32_t nowMs)
{
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
        currentAngle = sweepAngle(elapsed);
    }
    return BarrierEvent::None;
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

void BarrierDriver::begin(uint32_t nowMs, bool loopOccupied)
{
    current.begin(nowMs, loopOccupied);
}

BarrierEvent BarrierDriver::update(uint32_t nowMs, uint32_t openRequests, uint32_t passRequests, bool loopOccupied)
{
    if (openRequests != seenOpen)
    {
        seenOpen = openRequests;
        current.open(nowMs);
    }
    if (passRequests != seenPass)
    {
        passAccepted = current.vehiclePassed();
        seenPass = passRequests;
    }
    return current.update(nowMs, loopOccupied);
}

const Barrier& BarrierDriver::barrier() const
{
    return current;
}

uint32_t BarrierDriver::passesHandled() const
{
    return seenPass;
}

bool BarrierDriver::lastPassAccepted() const
{
    return passAccepted;
}

Debouncer::Debouncer(bool initial) : stable(initial), last(initial)
{
}

bool Debouncer::update(uint32_t nowMs, bool raw)
{
    if (raw != last)
    {
        last = raw;
        changedAtMs = nowMs;
    }
    else if (raw != stable && nowMs - changedAtMs >= LOOP_DEBOUNCE_MS)
    {
        stable = raw;
    }
    return stable;
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
