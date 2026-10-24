#pragma once

#include <stdint.h>

#include "barrier.h"
#include "schranken_app.h"

// Barrier in the same thread as the app (PC simulator, tests): the «Fahrzeug bleibt stehen» toggle is the
// only loop input. Call update() regularly with the millisecond clock.
class LocalBarrier : public BarrierPort
{
public:
    // Returns what happened in this update (safety stop, loop fault).
    BarrierEvent update(uint32_t nowMs);

    BarrierState state() const override;
    bool loopFault() const override;
    bool loopHeld() const override;
    // Opens at the time of the last update().
    void requestOpen() override;
    bool requestPass() override;
    void setLoopHeld(bool held) override;

private:
    Barrier barrier;
    bool held = false;
    uint32_t lastMs = 0;
};
