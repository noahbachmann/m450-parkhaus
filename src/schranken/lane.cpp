#include "lane.h"

#include <Arduino.h>

CoreLane::CoreLane(GateLane laneId, uint8_t servo, uint8_t loop) : id(laneId), servoPin(servo), loopPin(loop)
{
}

BarrierState CoreLane::state() const
{
    return static_cast<BarrierState>(currentState.load());
}

bool CoreLane::loopFault() const
{
    return fault.load();
}

bool CoreLane::loopHeld() const
{
    return held.load();
}

void CoreLane::requestOpen()
{
    openRequests.store(openRequests.load() + 1);
}

bool CoreLane::requestPass()
{
    uint32_t request = passRequests.load() + 1;
    passRequests.store(request);
    unsigned long startMs = millis();
    while (passesHandled.load() != request)
    {
        if (millis() - startMs >= PASS_ACK_TIMEOUT_MS)
        {
            return false;
        }
    }
    return passAccepted.load();
}

void CoreLane::setLoopHeld(bool loopHeld)
{
    held.store(loopHeld);
}

bool CoreLane::popEvent(BarrierEvent& event)
{
    return events.pop(event);
}

uint32_t CoreLane::droppedEvents() const
{
    return events.droppedCount();
}

void CoreLane::begin()
{
    pinMode(loopPin, INPUT_PULLUP);
    bool occupied = digitalRead(loopPin) == LOW;
    loop = Debouncer(occupied);
    driver.begin(millis(), occupied); // a car under the arm at boot: stay open
    writtenAngle = driver.barrier().angle();
    servo.attach(servoPin);
    servo.write(writtenAngle);
    update(millis());
}

void CoreLane::update(uint32_t nowMs)
{
    bool occupied = loop.update(nowMs, digitalRead(loopPin) == LOW) || held.load();
    BarrierEvent event = driver.update(nowMs, openRequests.load(), passRequests.load(), occupied);
    const Barrier& barrier = driver.barrier();
    if (barrier.angle() != writtenAngle)
    {
        writtenAngle = barrier.angle();
        servo.write(writtenAngle);
    }
    if (event != BarrierEvent::None)
    {
        events.push(event);
    }
    currentState.store(static_cast<int>(barrier.state()));
    fault.store(barrier.loopFault());
    passAccepted.store(driver.lastPassAccepted());
    passesHandled.store(driver.passesHandled());
}
