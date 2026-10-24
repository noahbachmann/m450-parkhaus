#include "local_barrier.h"

BarrierEvent LocalBarrier::update(uint32_t nowMs)
{
    lastMs = nowMs;
    return barrier.update(nowMs, held);
}

BarrierState LocalBarrier::state() const
{
    return barrier.state();
}

bool LocalBarrier::loopFault() const
{
    return barrier.loopFault();
}

bool LocalBarrier::loopHeld() const
{
    return held;
}

void LocalBarrier::requestOpen()
{
    barrier.open(lastMs);
}

bool LocalBarrier::requestPass()
{
    return barrier.vehiclePassed();
}

void LocalBarrier::setLoopHeld(bool loopHeld)
{
    held = loopHeld;
}
