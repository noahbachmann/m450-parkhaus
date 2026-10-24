#include "codec.h"

#include <ArduinoJson.h>
#include <cstdio>

#include "plate.h"

namespace
{
    struct Named
    {
        const char* name;
        int value;
    };

    const Named STATUS_NAMES[] = {
        {"open", static_cast<int>(SessionStatus::Open)},
        {"paid", static_cast<int>(SessionStatus::Paid)},
        {"closed", static_cast<int>(SessionStatus::Closed)},
        {"closed_free", static_cast<int>(SessionStatus::ClosedFree)},
        {"closed_unpaid", static_cast<int>(SessionStatus::ClosedUnpaid)},
    };

    template <size_t N>
    const char* nameOf(const Named (&table)[N], int value)
    {
        for (const Named& n : table)
        {
            if (n.value == value)
            {
                return n.name;
            }
        }
        return "";
    }

    template <size_t N>
    bool valueOf(const Named (&table)[N], JsonVariantConst v, int& value)
    {
        if (!v.is<const char*>())
        {
            return false;
        }
        std::string s = v.as<std::string>();
        for (const Named& n : table)
        {
            if (s == n.name)
            {
                value = n.value;
                return true;
            }
        }
        return false;
    }

    // "TX-000042" -> 42; 0 for an empty or other reference.
    uint32_t txNumber(const std::string& ref)
    {
        unsigned long n = 0;
        return std::sscanf(ref.c_str(), "TX-%lu", &n) == 1 ? static_cast<uint32_t>(n) : 0;
    }

    bool parseSession(JsonObjectConst o, ParkingSession& s)
    {
        if (!o["kz"].is<const char*>() || !o["entry"].is<int64_t>() || !o["exit"].is<int64_t>() ||
            !o["amount"].is<uint32_t>() || !o["cash"].is<uint32_t>() || !o["tx"].is<const char*>())
        {
            return false;
        }
        int status = 0;
        if (!valueOf(STATUS_NAMES, o["status"], status) || !o["method"].is<const char*>() ||
            !paymentMethodFromName(o["method"].as<std::string>(), s.method))
        {
            return false;
        }
        s.plate = o["kz"].as<std::string>();
        s.entry = o["entry"].as<int64_t>();
        s.exit = o["exit"].as<int64_t>();
        s.amountRappen = o["amount"].as<uint32_t>();
        s.cashPaidRappen = o["cash"].as<uint32_t>();
        s.txRef = o["tx"].as<std::string>();
        s.status = static_cast<SessionStatus>(status);
        return isValidPlate(s.plate);
    }
}

const char* sessionStatusName(SessionStatus status)
{
    return nameOf(STATUS_NAMES, static_cast<int>(status));
}

std::string registrationsToJson(const RegistrationList& list)
{
    JsonDocument doc;
    JsonArray arr = doc["registrations"].to<JsonArray>();
    for (const Registration& r : list.all())
    {
        JsonObject o = arr.add<JsonObject>();
        o["name"] = r.name;
        o["kz"] = r.plate;
    }
    std::string out;
    serializeJson(doc, out);
    return out;
}

bool registrationsFromJson(const std::string& json, RegistrationList& list)
{
    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok || !doc["registrations"].is<JsonArrayConst>())
    {
        return false;
    }
    RegistrationList loaded;
    for (JsonVariantConst v : doc["registrations"].as<JsonArrayConst>())
    {
        if (!v["name"].is<const char*>() || !v["kz"].is<const char*>())
        {
            return false;
        }
        if (loaded.add(v["name"].as<std::string>(), v["kz"].as<std::string>()) != RegistrationResult::Ok)
        {
            return false;
        }
    }
    list.clear(); // add() again instead of assigning, so list.revision() moves on
    for (const Registration& r : loaded.all())
    {
        list.add(r.name, r.plate);
    }
    return true;
}

std::string sessionsToJson(const SessionStore& store)
{
    JsonDocument doc;
    sessionsToDoc(store, doc);
    std::string out;
    serializeJson(doc, out);
    return out;
}

void sessionsToDoc(const SessionStore& store, JsonDocument& doc)
{
    doc["nextTx"] = store.nextTxNumber();
    JsonArray arr = doc["sessions"].to<JsonArray>();
    for (const ParkingSession& s : store.all())
    {
        JsonObject o = arr.add<JsonObject>();
        o["kz"] = s.plate;
        o["entry"] = s.entry;
        o["exit"] = s.exit;
        o["amount"] = s.amountRappen;
        o["cash"] = s.cashPaidRappen;
        o["method"] = paymentMethodName(s.method);
        o["tx"] = s.txRef;
        o["status"] = sessionStatusName(s.status);
    }
}

bool sessionsFromJson(const std::string& json, SessionStore& store)
{
    JsonDocument doc;
    return deserializeJson(doc, json) == DeserializationError::Ok && sessionsFromDoc(doc, store);
}

bool sessionsFromDoc(const JsonDocument& doc, SessionStore& store)
{
    if (!doc["nextTx"].is<uint32_t>() || !doc["sessions"].is<JsonArrayConst>())
    {
        return false;
    }
    JsonArrayConst arr = doc["sessions"].as<JsonArrayConst>();
    if (arr.size() > MAX_SESSIONS)
    {
        return false;
    }
    std::vector<ParkingSession> loaded;
    uint32_t nextTx = doc["nextTx"].as<uint32_t>();
    for (JsonVariantConst v : arr)
    {
        ParkingSession s;
        if (!v.is<JsonObjectConst>() || !parseSession(v.as<JsonObjectConst>(), s))
        {
            return false;
        }
        for (const ParkingSession& other : loaded)
        {
            if (other.plate == s.plate && isActive(other.status) && isActive(s.status))
            {
                return false;
            }
        }
        if (txNumber(s.txRef) >= nextTx)
        {
            nextTx = txNumber(s.txRef) + 1; // never hand out a stored reference again
        }
        loaded.push_back(s);
    }
    store.restore(std::move(loaded), nextTx);
    return true;
}

std::string eventsToJson(const EventLog& log)
{
    JsonDocument doc;
    JsonArray arr = doc["events"].to<JsonArray>();
    for (const Event& e : log.all())
    {
        JsonObject o = arr.add<JsonObject>();
        o["t"] = e.time;
        o["type"] = eventTypeName(e.type);
        o["lane"] = gateLaneName(e.lane);
        o["kz"] = e.plate;
        o["user"] = e.user;
        o["reason"] = e.reason;
        o["note"] = e.note;
    }
    std::string out;
    serializeJson(doc, out);
    return out;
}

bool eventsFromJson(const std::string& json, EventLog& log)
{
    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok || !doc["events"].is<JsonArrayConst>())
    {
        return false;
    }
    std::vector<Event> loaded;
    for (JsonVariantConst v : doc["events"].as<JsonArrayConst>())
    {
        Event e;
        if (!v["t"].is<int64_t>() || !v["type"].is<const char*>() || !v["lane"].is<const char*>() ||
            !v["kz"].is<const char*>() || !v["user"].is<const char*>() || !v["reason"].is<const char*>() ||
            !v["note"].is<const char*>() || !eventTypeFromName(v["type"].as<std::string>(), e.type) ||
            !gateLaneFromName(v["lane"].as<std::string>(), e.lane))
        {
            return false;
        }
        e.time = v["t"].as<int64_t>();
        e.plate = v["kz"].as<std::string>();
        e.user = v["user"].as<std::string>();
        e.reason = v["reason"].as<std::string>();
        e.note = v["note"].as<std::string>();
        loaded.push_back(e);
    }
    log.restore(std::move(loaded));
    return true;
}
