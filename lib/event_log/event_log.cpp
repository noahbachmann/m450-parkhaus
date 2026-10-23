#include "event_log.h"

namespace
{
    struct TypeInfo
    {
        EventType type;
        const char* name;
        const char* label;
    };

    const TypeInfo TYPES[] = {
        {EventType::SafetyStop, "safety_stop", "Sicherheitsabschaltung"},
        {EventType::LoopFault, "loop_fault", "Störung Sicherungsschleife"},
        {EventType::RemoteOpening, "remote_opening", "Fernöffnung"},
        {EventType::KasseUnreachable, "kasse_unreachable", "Kasse nicht erreichbar"},
        {EventType::CallButton, "call_button", "Ruftaste"},
    };

    const TypeInfo& infoOf(EventType type)
    {
        for (const TypeInfo& t : TYPES)
        {
            if (t.type == type)
            {
                return t;
            }
        }
        return TYPES[0];
    }
}

void EventLog::add(const Event& event)
{
    if (events.size() >= MAX_EVENTS)
    {
        events.erase(events.begin());
    }
    events.push_back(event);
}

const std::vector<Event>& EventLog::all() const
{
    return events;
}

void EventLog::clear()
{
    events.clear();
}

void EventLog::restore(std::vector<Event> loaded)
{
    if (loaded.size() > MAX_EVENTS)
    {
        loaded.erase(loaded.begin(), loaded.end() - MAX_EVENTS);
    }
    events = std::move(loaded);
}

const char* gateLaneLabel(GateLane lane)
{
    switch (lane)
    {
    case GateLane::Entry:
        return "Einfahrt";
    case GateLane::Exit:
        return "Ausfahrt";
    case GateLane::None:
        break;
    }
    return "";
}

const char* eventTypeLabel(EventType type)
{
    return infoOf(type).label;
}

const char* eventTypeName(EventType type)
{
    return infoOf(type).name;
}

bool eventTypeFromName(const std::string& name, EventType& type)
{
    for (const TypeInfo& t : TYPES)
    {
        if (name == t.name)
        {
            type = t.type;
            return true;
        }
    }
    return false;
}

const char* gateLaneName(GateLane lane)
{
    switch (lane)
    {
    case GateLane::Entry:
        return "entry";
    case GateLane::Exit:
        return "exit";
    case GateLane::None:
        break;
    }
    return "";
}

bool gateLaneFromName(const std::string& name, GateLane& lane)
{
    if (name == "entry")
    {
        lane = GateLane::Entry;
    }
    else if (name == "exit")
    {
        lane = GateLane::Exit;
    }
    else if (name.empty())
    {
        lane = GateLane::None;
    }
    else
    {
        return false;
    }
    return true;
}
