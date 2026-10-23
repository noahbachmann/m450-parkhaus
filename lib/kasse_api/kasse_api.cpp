#include "kasse_api.h"

#include <ArduinoJson.h>

#include "codec.h"
#include "plate.h"

namespace
{
    ApiResponse respond(int status, const JsonDocument& doc)
    {
        std::string body;
        serializeJson(doc, body);
        return {status, body};
    }

    ApiResponse error(int status, const char* code)
    {
        JsonDocument doc;
        doc["error"] = code;
        return respond(status, doc);
    }

    // Parses {"kz":"..."} plus optional fields. False if the body is not a JSON object with a string kz.
    bool parseBody(const std::string& body, JsonDocument& doc, std::string& plate)
    {
        if (deserializeJson(doc, body) != DeserializationError::Ok || !doc.is<JsonObject>() ||
            !doc["kz"].is<const char*>())
        {
            return false;
        }
        plate = normalizePlate(doc["kz"].as<std::string>());
        return true;
    }
}

ApiResponse handleStatus(const SessionStore& store, const std::string& kzParam, int64_t now)
{
    std::string plate = normalizePlate(kzParam);
    if (!isValidPlate(plate))
    {
        return error(400, "invalid_plate");
    }
    JsonDocument doc;
    doc["kz"] = plate;
    const ParkingSession* s = store.findActive(plate);
    if (s == nullptr)
    {
        doc["status"] = "unbekannt";
        return respond(200, doc);
    }
    uint32_t fee = s->amountRappen;
    if (s->status == SessionStatus::Open && !currentFee(*s, now, fee))
    {
        fee = 0;
    }
    doc["status"] = s->status == SessionStatus::Paid ? "bezahlt" : "unbezahlt";
    doc["fee"] = fee;
    doc["entry"] = s->entry;
    return respond(200, doc);
}

ApiResponse handleEntry(SessionStore& store, const std::string& body, int64_t now, bool timeSynced)
{
    JsonDocument req;
    std::string plate;
    if (!parseBody(body, req, plate) || (!req["entry"].isNull() && !req["entry"].is<int64_t>()))
    {
        return error(400, "bad_request");
    }
    if (!isValidPlate(plate))
    {
        return error(400, "invalid_plate");
    }
    if (!timeSynced)
    {
        return error(503, "time_not_synced");
    }
    std::optional<int64_t> backdated;
    if (!req["entry"].isNull())
    {
        backdated = req["entry"].as<int64_t>();
    }
    switch (store.openEntry(plate, now, backdated))
    {
    case EntryResult::Ok:
        break;
    case EntryResult::AlreadyOpen:
        return error(409, "already_open");
    case EntryResult::EntryInFuture:
        return error(400, "entry_in_future");
    case EntryResult::Full:
        return error(503, "store_full");
    case EntryResult::InvalidPlate:
        return error(400, "invalid_plate");
    }
    JsonDocument doc;
    doc["kz"] = plate;
    doc["entry"] = store.findActive(plate)->entry;
    return respond(201, doc);
}

ApiResponse handleExit(SessionStore& store, const std::string& body, int64_t now, bool timeSynced)
{
    JsonDocument req;
    std::string plate;
    if (!parseBody(body, req, plate) || (!req["override"].isNull() && !req["override"].is<bool>()))
    {
        return error(400, "bad_request");
    }
    if (!isValidPlate(plate))
    {
        return error(400, "invalid_plate");
    }
    if (!timeSynced)
    {
        return error(503, "time_not_synced");
    }
    ExitResult r = req["override"].as<bool>() ? store.forceExit(plate, now) : store.tryExit(plate, now);
    JsonDocument doc;
    switch (r.kind)
    {
    case ExitResult::Kind::Closed:
    case ExitResult::Kind::ClosedFree:
    case ExitResult::Kind::ClosedUnpaid:
        doc["kz"] = plate;
        doc["status"] = sessionStatusName(r.kind == ExitResult::Kind::Closed       ? SessionStatus::Closed
                                          : r.kind == ExitResult::Kind::ClosedFree ? SessionStatus::ClosedFree
                                                                                   : SessionStatus::ClosedUnpaid);
        doc["exit"] = r.exit;
        doc["fee"] = r.feeRappen;
        return respond(200, doc);
    case ExitResult::Kind::Unpaid:
        doc["error"] = "unpaid";
        doc["fee"] = r.feeRappen;
        return respond(402, doc);
    case ExitResult::Kind::NoSession:
        return error(404, "no_session");
    case ExitResult::Kind::TimeError:
        return error(503, "time_not_synced");
    case ExitResult::Kind::InvalidPlate:
        break;
    }
    return error(400, "invalid_plate");
}
