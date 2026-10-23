#include "gate.h"

#include <ArduinoJson.h>
#include <stdint.h>

#include "format.h"
#include "plate.h"

namespace
{
    const char* UNREACHABLE = "Kasse nicht erreichbar – bitte Ruftaste drücken";
    const char* UNEXPECTED = "Unerwartete Antwort der Kasse – bitte Ruftaste drücken";
    const char* TIME_NOT_SYNCED = "Kasse: Uhrzeit nicht synchronisiert – bitte Ruftaste drücken";

    GateResult closed(const std::string& plate, const std::string& message)
    {
        GateResult r;
        r.plate = plate;
        r.message = message;
        return r;
    }

    GateResult unreachable(const std::string& plate)
    {
        GateResult r = closed(plate, UNREACHABLE);
        r.unreachable = true;
        return r;
    }

    GateResult opened(const std::string& plate, const std::string& message)
    {
        GateResult r;
        r.open = true;
        r.cls = "ok";
        r.plate = plate;
        r.message = message;
        return r;
    }

    // False if the body is not a JSON object.
    bool parseReply(const HttpReply& reply, JsonDocument& doc)
    {
        return deserializeJson(doc, reply.body) == DeserializationError::Ok && doc.is<JsonObject>();
    }

    std::string errorCode(const HttpReply& reply)
    {
        JsonDocument doc;
        if (!parseReply(reply, doc) || !doc["error"].is<const char*>())
        {
            return "";
        }
        return doc["error"].as<std::string>();
    }

    bool checkPlate(const std::string& plate, GateResult& rejected)
    {
        if (plate.empty())
        {
            rejected = closed(plate, "Bitte Kennzeichen eingeben");
            return false;
        }
        if (!isValidPlate(plate))
        {
            rejected = closed(plate, "Ungültiges Kennzeichen – erwartet 2 Buchstaben und 1–6 Ziffern");
            return false;
        }
        return true;
    }
}

GateResult entryRequest(const RegistrationList& registrations, const std::string& rawPlate,
                        const std::string& entryTimeInput, KasseLink& kasse)
{
    std::string plate = normalizePlate(rawPlate);
    GateResult rejected;
    if (!checkPlate(plate, rejected))
    {
        return rejected;
    }
    if (!registrations.contains(plate))
    {
        return closed(plate, plate + " – nicht registriert, bitte Ruftaste drücken");
    }

    JsonDocument req;
    req["kz"] = plate;
    if (entryTimeInput.find_first_not_of(" \t") != std::string::npos)
    {
        int64_t entry = 0;
        if (!parseLocalDateTime(entryTimeInput, entry))
        {
            return closed(plate, "Ungültige Einfahrtszeit – Format TT.MM.JJJJ HH:MM");
        }
        req["entry"] = entry;
    }
    std::string body;
    serializeJson(req, body);

    HttpReply reply = kasse.postJson("/api/entry", body);
    if (reply.status <= 0)
    {
        return unreachable(plate);
    }
    if (reply.status == 201)
    {
        JsonDocument doc;
        if (parseReply(reply, doc) && doc["kz"].is<const char*>() && doc["kz"].as<std::string>() == plate &&
            doc["entry"].is<int64_t>())
        {
            return opened(plate, plate + " – Willkommen, Einfahrt frei");
        }
        return closed(plate, UNEXPECTED);
    }
    std::string code = errorCode(reply);
    if (reply.status == 409 && code == "already_open")
    {
        return closed(plate, "Parkvorgang bereits offen – bitte Ruftaste drücken");
    }
    if (reply.status == 400 && code == "entry_in_future")
    {
        return closed(plate, "Einfahrtszeit liegt in der Zukunft");
    }
    if (reply.status == 503 && code == "time_not_synced")
    {
        return closed(plate, TIME_NOT_SYNCED);
    }
    if (reply.status == 503 && code == "store_full")
    {
        return closed(plate, "Kasse: Speicher voll – bitte Ruftaste drücken");
    }
    return closed(plate, UNEXPECTED);
}

GateResult exitRequest(const std::string& rawPlate, KasseLink& kasse)
{
    std::string plate = normalizePlate(rawPlate);
    GateResult rejected;
    if (!checkPlate(plate, rejected))
    {
        return rejected;
    }

    JsonDocument req;
    req["kz"] = plate;
    std::string body;
    serializeJson(req, body);

    HttpReply reply = kasse.postJson("/api/exit", body);
    if (reply.status <= 0)
    {
        return unreachable(plate);
    }
    JsonDocument doc;
    bool parsed = parseReply(reply, doc);
    if (reply.status == 200)
    {
        std::string status = parsed && doc["status"].is<const char*>() ? doc["status"].as<std::string>() : "";
        if (parsed && doc["kz"].is<const char*>() && doc["kz"].as<std::string>() == plate &&
            (status == "closed" || status == "closed_free"))
        {
            return opened(plate, plate + " – Gute Fahrt, Ausfahrt frei");
        }
        return closed(plate, UNEXPECTED);
    }
    std::string code = parsed && doc["error"].is<const char*>() ? doc["error"].as<std::string>() : "";
    if (reply.status == 402 && code == "unpaid" && doc["fee"].is<uint32_t>())
    {
        GateResult r = closed(plate, "Gebühr " + formatChf(doc["fee"].as<uint32_t>()) +
                                         " nicht bezahlt – bitte am Automaten bezahlen");
        r.unpaid = true;
        r.feeRappen = doc["fee"].as<uint32_t>();
        return r;
    }
    if (reply.status == 404 && code == "no_session")
    {
        return closed(plate, "Kein Parkvorgang gefunden – bitte Ruftaste drücken");
    }
    if (reply.status == 503 && code == "time_not_synced")
    {
        return closed(plate, TIME_NOT_SYNCED);
    }
    return closed(plate, UNEXPECTED);
}
