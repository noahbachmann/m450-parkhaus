#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

constexpr size_t MAX_EVENTS = 100;

enum class GateLane
{
    None,
    Entry,
    Exit,
};

enum class EventType
{
    SafetyStop,       // «Sicherheitsabschaltung»
    LoopFault,        // «Störung Sicherungsschleife»
    RemoteOpening,    // «Fernöffnung»
    KasseUnreachable, // «Kasse nicht erreichbar»
    CallButton,       // «Ruftaste»
    PaymentAlert,     // «Zahlung 5× abgelehnt»
};

struct Event
{
    int64_t time = 0; // epoch seconds (UTC)
    EventType type = EventType::SafetyStop;
    GateLane lane = GateLane::None;
    std::string plate;
    std::string user;
    std::string reason;
    std::string note;
};

// Event log on Pico 2 (test object 11). Oldest entries are dropped beyond MAX_EVENTS.
class EventLog
{
public:
    void add(const Event& event);
    const std::vector<Event>& all() const;
    void clear();
    void restore(std::vector<Event> loaded);

private:
    std::vector<Event> events;
};

// «Einfahrt», «Ausfahrt», "" for None.
const char* gateLaneLabel(GateLane lane);
const char* eventTypeLabel(EventType type);
// All types in display order (filter list).
const EventType* allEventTypes(size_t& count);
// File/form names: "safety_stop", "loop_fault", "remote_opening", "kasse_unreachable", "call_button",
// "payment_alert".
const char* eventTypeName(EventType type);
bool eventTypeFromName(const std::string& name, EventType& type);
const char* gateLaneName(GateLane lane); // "", "entry", "exit"
bool gateLaneFromName(const std::string& name, GateLane& lane);
