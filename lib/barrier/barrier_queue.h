#pragma once

#include <stdint.h>
#include <atomic>

#include "barrier.h"

// Barrier events from core 1 to core 0 on Pico 2 (single producer, single consumer, lock-free on the RP2040).
// Core 0 logs them with the NTP time and writes the log to flash; core 1 must not touch the log or LittleFS.
class BarrierEventQueue
{
public:
    static constexpr uint32_t SIZE = 8;

    // Core 1. False if the queue is full (the event is dropped and counted).
    bool push(BarrierEvent e)
    {
        uint32_t h = head.load();
        if (h - tail.load() >= SIZE)
        {
            dropped.store(dropped.load() + 1);
            return false;
        }
        items[h % SIZE] = e;
        head.store(h + 1);
        return true;
    }

    // Core 0.
    bool pop(BarrierEvent& e)
    {
        uint32_t t = tail.load();
        if (t == head.load())
        {
            return false;
        }
        e = items[t % SIZE];
        tail.store(t + 1);
        return true;
    }

    // Events lost because core 0 didn't drain in time.
    uint32_t droppedCount() const
    {
        return dropped.load();
    }

private:
    BarrierEvent items[SIZE] = {};
    std::atomic<uint32_t> head{0};    // written by core 1
    std::atomic<uint32_t> tail{0};    // written by core 0
    std::atomic<uint32_t> dropped{0}; // written by core 1
};
