#include "kasse_api.h"

#include <ArduinoJson.h>

#include "codec.h"
#include "payment.h"
#include "plate.h"
#include "testdata.h"

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
    case EntryResult::Repeated:
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

ApiResponse handlePay(SessionStore& store, const std::string& body, int64_t now, bool timeSynced)
{
    JsonDocument req;
    std::string plate;
    if (!parseBody(body, req, plate) || !req["method"].is<const char*>())
    {
        return error(400, "bad_request");
    }
    std::string name = req["method"].as<std::string>();
    PayMethod method;
    if (name == "card_ok")
    {
        method = PayMethod::CardOk;
    }
    else if (name == "card_declined")
    {
        method = PayMethod::CardDeclined;
    }
    else if (name == "twint")
    {
        method = PayMethod::Twint;
    }
    else if (name == "cash" && (req["amount"].isNull() || req["amount"].is<uint32_t>()))
    {
        method = PayMethod::Cash;
    }
    else
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
    PaymentResult r = pay(store, plate, method, now, req["amount"].as<uint32_t>());
    switch (r.kind)
    {
    case PaymentResult::Kind::Paid:
    {
        JsonDocument doc;
        doc["kz"] = plate;
        doc["status"] = "bezahlt";
        doc["amount"] = r.amountRappen;
        doc["method"] = r.method == PaymentMethod::Twint ? "twint" : r.method == PaymentMethod::Cash ? "cash" : "card";
        doc["tx"] = r.txRef;
        if (r.method == PaymentMethod::Cash)
        {
            doc["change"] = r.changeRappen;
        }
        return respond(200, doc);
    }
    case PaymentResult::Kind::PartialCash:
    {
        JsonDocument doc;
        doc["kz"] = plate;
        doc["status"] = "unbezahlt";
        doc["amount"] = r.amountRappen;
        doc["remaining"] = r.remainingRappen;
        return respond(200, doc);
    }
    case PaymentResult::Kind::InvalidAmount:
        return error(400, "invalid_amount");
    case PaymentResult::Kind::Declined:
        return error(402, "declined");
    case PaymentResult::Kind::AlreadyPaid:
        return error(409, "already_paid");
    case PaymentResult::Kind::NothingToPay:
        return error(409, "nothing_to_pay");
    case PaymentResult::Kind::NoSession:
        return error(404, "no_session");
    case PaymentResult::Kind::TimeError:
        return error(503, "time_not_synced");
    case PaymentResult::Kind::InvalidPlate:
        break;
    }
    return error(400, "invalid_plate");
}

ApiResponse handleSessions(const SessionStore& store, int64_t now, bool timeSynced)
{
    JsonDocument doc;
    deserializeJson(doc, sessionsToJson(store));
    doc["now"] = now;
    doc["synced"] = timeSynced;
    return respond(200, doc);
}

ApiResponse handleReset(SessionStore& store)
{
    loadDefaultSessions(store);
    JsonDocument doc;
    doc["status"] = "reset";
    return respond(200, doc);
}
