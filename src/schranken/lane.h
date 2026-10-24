#pragma once

#include <Servo.h>
#include <atomic>
#include <stdint.h>

#include "barrier.h"
#include "barrier_queue.h"
#include "event_log.h"
#include "schranken_app.h"

// One barrier of Pico 2: servo and safety loop run on core 1 (setup1/loop1), so a blocking HTTP call on core 0
// never delays a safety stop. The app on core 0 sees it as a BarrierPort. The cores share only the atomics below,
// each written by one core; core 1 never touches the event log or LittleFS.
class CoreLane : public BarrierPort
{
public:
    CoreLane(GateLane laneId, uint8_t servoPin, uint8_t loopPin);

    // Core 0 (BarrierPort).
    BarrierState state() const override;
    bool loopFault() const override;
    bool loopHeld() const override;
    void requestOpen() override;
    // Waits up to PASS_ACK_TIMEOUT_MS for core 1 to apply the pass; false on timeout.
    bool requestPass() override;
    void setLoopHeld(bool held) override;
    // Core 0: next safety stop / loop fault from core 1.
    bool popEvent(BarrierEvent& event);
    uint32_t droppedEvents() const;

    // Core 1.
    void begin();
    void update(uint32_t nowMs);

    const GateLane id;

private:
    static constexpr uint32_t PASS_ACK_TIMEOUT_MS = 20;

    const uint8_t servoPin;
    const uint8_t loopPin;

    // core 0 -> core 1
    std::atomic<uint32_t> openRequests{0};
    std::atomic<uint32_t> passRequests{0};
    std::atomic<bool> held{false};
    // core 1 -> core 0
    std::atomic<int> currentState{static_cast<int>(BarrierState::Closed)};
    std::atomic<bool> fault{false};
    std::atomic<bool> passAccepted{false};
    std::atomic<uint32_t> passesHandled{0}; // stored last, after passAccepted
    BarrierEventQueue events;

    // core 1 only
    Servo servo;
    BarrierDriver driver;
    int writtenAngle = -1;
};
