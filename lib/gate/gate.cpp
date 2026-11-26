#include "gate.h"

#include <ArduinoJson.h>
#include <stddef.h>
#include <stdint.h>

#include "format.h"
#include "http_status.h"
#include "plate.h"

const char* const UNEXPECTED_REPLY_TEXT = "Unerwartete Antwort der Kasse – bitte Ruftaste drücken";

namespace
{
    const char* const UNREACHABLE_TEXT = "Kasse nicht erreichbar – bitte Ruftaste drücken";
    const char* const TIME_NOT_SYNCED_TEXT = "Kasse: Uhrzeit nicht synchronisiert – bitte Ruftaste drücken";

    // A Pico 1 error reply and the terminal text it is shown with.
    struct ErrorText
    {
        int status;
        const char* code;
        const char* text;
    };

    const ErrorText ENTRY_ERRORS[] = {
        {HTTP_CONFLICT, "already_open", "Parkvorgang bereits offen – bitte Ruftaste drücken"},
        {HTTP_BAD_REQUEST, "entry_in_future", "Einfahrtszeit liegt in der Zukunft"},
        {HTTP_UNAVAILABLE, "time_not_synced", TIME_NOT_SYNCED_TEXT},
        {HTTP_UNAVAILABLE, "store_full", "Kasse: Speicher voll – bitte Ruftaste drücken"},
    };

    const ErrorText EXIT_ERRORS[] = {
        {HTTP_NOT_FOUND, "no_session", "Kein Parkvorgang gefunden – bitte Ruftaste drücken"},
        {HTTP_UNAVAILABLE, "time_not_synced", TIME_NOT_SYNCED_TEXT},
    };

    GateResult refused(const std::string& plate, const std::string& message)
    {
        GateResult r;
        r.plate = plate;
        r.message = message;
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

    // The reply is about this plate (Pico 1 echoes "kz").
    bool confirmsPlate(const JsonDocument& doc, const std::string& plate)
    {
        return doc["kz"].is<const char*>() && doc["kz"].as<std::string>() == plate;
    }

    bool checkPlate(const std::string& plate, GateResult& rejected)
    {
        if (plate.empty())
        {
            rejected = refused(plate, "Bitte Kennzeichen eingeben");
            return false;
        }
        if (!isValidPlate(plate))
        {
            rejected = refused(plate, INVALID_PLATE_TEXT);
            return false;
        }
        return true;
    }

    HttpReply postToKasse(KasseLink& kasse, const char* path, const JsonDocument& req)
    {
        std::string body;
        serializeJson(req, body);
        return kasse.postJson(path, body);
    }

    // Text for a known error reply, «Unerwartete Antwort der Kasse» for anything else.
    template <size_t N>
    GateResult refusedByKasse(const std::string& plate, int status, const std::string& code,
                              const ErrorText (&errors)[N])
    {
        for (const ErrorText& error : errors)
        {
            if (status == error.status && code == error.code)
            {
                return refused(plate, error.text);
            }
        }
        return refused(plate, UNEXPECTED_REPLY_TEXT);
    }

    // 402 unpaid: «Gebühr CHF 6.00 nicht bezahlt – …», after partial cash «Restbetrag CHF 1.50 von CHF 4.00 …».
    GateResult unpaidResult(const std::string& plate, const JsonDocument& doc)
    {
        uint32_t fee = doc["fee"].as<uint32_t>();
        uint32_t cash = doc["cash"].is<uint32_t>() ? doc["cash"].as<uint32_t>() : 0;
        if (cash >= fee)
        {
            cash = 0; // inconsistent reply: ask for the whole fee
        }
        std::string due = cash > 0 ? "Restbetrag " + formatChf(fee - cash) + " von " + formatChf(fee)
                                   : "Gebühr " + formatChf(fee);
        GateResult r = refused(plate, due + " nicht bezahlt – bitte am Automaten bezahlen");
        r.unpaid = true;
        r.feeRappen = fee;
        r.cashRappen = cash;
        return r;
    }
}

GateResult kasseUnreachable(const std::string& plate)
{
    GateResult r = refused(plate, UNREACHABLE_TEXT);
    r.unreachable = true;
    return r;
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
        return refused(plate, plate + " – nicht registriert, bitte Ruftaste drücken");
    }
    JsonDocument req;
    req["kz"] = plate;
    if (!trim(entryTimeInput).empty())
    {
        int64_t entry = 0;
        if (!parseLocalDateTime(entryTimeInput, entry))
        {
            return refused(plate, "Ungültige Einfahrtszeit – Format TT.MM.JJJJ HH:MM");
        }
        req["entry"] = entry;
    }

    HttpReply reply = postToKasse(kasse, "/api/entry", req);
    if (reply.status <= 0)
    {
        return kasseUnreachable(plate);
    }
    if (reply.status != HTTP_CREATED)
    {
        return refusedByKasse(plate, reply.status, errorCode(reply), ENTRY_ERRORS);
    }
    JsonDocument doc;
    if (!parseReply(reply, doc) || !confirmsPlate(doc, plate) || !doc["entry"].is<int64_t>())
    {
        return refused(plate, UNEXPECTED_REPLY_TEXT);
    }
    return opened(plate, plate + " – Willkommen, Einfahrt frei");
}

GateResult laneBusy(const std::string& rawPlate)
{
    return refused(normalizePlate(rawPlate), "Bitte warten, bis das vorherige Fahrzeug durchgefahren ist");
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

    HttpReply reply = postToKasse(kasse, "/api/exit", req);
    if (reply.status <= 0)
    {
        return kasseUnreachable(plate);
    }
    JsonDocument doc;
    bool parsed = parseReply(reply, doc);
    std::string code = errorCode(reply);
    if (reply.status == HTTP_PAYMENT_REQUIRED && code == "unpaid" && doc["fee"].is<uint32_t>())
    {
        return unpaidResult(plate, doc);
    }
    if (reply.status != HTTP_OK)
    {
        return refusedByKasse(plate, reply.status, code, EXIT_ERRORS);
    }
    std::string status = parsed && doc["status"].is<const char*>() ? doc["status"].as<std::string>() : "";
    if (!parsed || !confirmsPlate(doc, plate) || (status != "closed" && status != "closed_free"))
    {
        return refused(plate, UNEXPECTED_REPLY_TEXT);
    }
    return opened(plate, plate + " – Gute Fahrt, Ausfahrt frei");
}
