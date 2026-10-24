#include "kasse_client.h"

#include <ArduinoJson.h>

#include "codec.h"
#include "kasse_view.h"
#include "payment.h"
#include "plate.h"

namespace
{
    const char* UNREACHABLE = "Kasse nicht erreichbar – bitte später erneut versuchen";
    const char* UNEXPECTED = "Unerwartete Antwort der Kasse";

    PaymentMethod methodOf(const std::string& name)
    {
        return name == "twint" ? PaymentMethod::Twint : name == "cash" ? PaymentMethod::Cash : PaymentMethod::Card;
    }

    // Reply of POST /api/pay -> the payment result it stands for. False if the reply makes no sense.
    bool paymentFromReply(const HttpReply& reply, const std::string& plate, PaymentResult& out)
    {
        using Kind = PaymentResult::Kind;
        JsonDocument doc;
        if (deserializeJson(doc, reply.body) != DeserializationError::Ok || !doc.is<JsonObject>())
        {
            return false;
        }
        if (reply.status == 200)
        {
            if (!doc["kz"].is<const char*>() || doc["kz"].as<std::string>() != plate ||
                !doc["amount"].is<uint32_t>() || !doc["status"].is<const char*>())
            {
                return false;
            }
            std::string status = doc["status"].as<std::string>();
            if (status == "unbezahlt" && doc["remaining"].is<uint32_t>())
            {
                out = {Kind::PartialCash, doc["amount"].as<uint32_t>(), doc["remaining"].as<uint32_t>()};
                return true;
            }
            if (status != "bezahlt" || !doc["method"].is<const char*>() || !doc["tx"].is<const char*>())
            {
                return false;
            }
            out = {Kind::Paid, doc["amount"].as<uint32_t>(), 0, doc["change"].as<uint32_t>(),
                   methodOf(doc["method"].as<std::string>()), doc["tx"].as<std::string>()};
            return true;
        }
        std::string code = doc["error"].is<const char*>() ? doc["error"].as<std::string>() : "";
        Kind kind;
        if (reply.status == 402 && code == "declined")
        {
            kind = Kind::Declined;
        }
        else if (reply.status == 409 && code == "already_paid")
        {
            kind = Kind::AlreadyPaid;
        }
        else if (reply.status == 409 && code == "nothing_to_pay")
        {
            kind = Kind::NothingToPay;
        }
        else if (reply.status == 404 && code == "no_session")
        {
            kind = Kind::NoSession;
        }
        else if (reply.status == 400 && code == "invalid_plate")
        {
            kind = Kind::InvalidPlate;
        }
        else if (reply.status == 400 && code == "invalid_amount")
        {
            kind = Kind::InvalidAmount;
        }
        else if (reply.status == 503 && code == "time_not_synced")
        {
            kind = Kind::TimeError;
        }
        else
        {
            return false;
        }
        out = {kind};
        return true;
    }
}

KasseSnapshot fetchSessions(KasseLink& kasse)
{
    KasseSnapshot snap;
    HttpReply reply = kasse.getJson("/api/sessions");
    if (reply.status != 200 || !sessionsFromJson(reply.body, snap.sessions))
    {
        return snap;
    }
    JsonDocument doc;
    deserializeJson(doc, reply.body);
    snap.reachable = doc["now"].is<int64_t>();
    snap.synced = snap.reachable && doc["synced"].as<bool>();
    snap.now = doc["now"].as<int64_t>();
    return snap;
}

FormResult payAtKasse(KasseLink& kasse, const std::string& kz, const std::string& method, const std::string& amount)
{
    FormResult r;
    PayMethod m;
    if (!parsePayMethod(method, m))
    {
        r.notice = {"err", "Bitte Zahlungsmittel wählen"};
        return r;
    }
    std::string plate = normalizePlate(kz);
    JsonDocument req;
    req["kz"] = plate;
    req["method"] = method;
    if (m == PayMethod::Cash)
    {
        uint32_t cash = 0;
        if (!parseChf(amount, cash))
        {
            r.notice = {"err", paymentMessage({PaymentResult::Kind::InvalidAmount}, plate)};
            return r;
        }
        req["amount"] = cash;
    }
    std::string body;
    serializeJson(req, body);
    HttpReply reply = kasse.postJson("/api/pay", body);
    if (reply.status <= 0)
    {
        r.notice = {"err", UNREACHABLE};
        return r;
    }
    PaymentResult p{PaymentResult::Kind::TimeError};
    if (!paymentFromReply(reply, plate, p))
    {
        r.notice = {"err", std::string(UNEXPECTED) + " (" + std::to_string(reply.status) + ")"};
        return r;
    }
    r.changed = paymentSucceeded(p);
    const char* cls = p.kind == PaymentResult::Kind::Paid          ? "ok"
                      : p.kind == PaymentResult::Kind::PartialCash ? "info"
                                                                   : "err";
    r.notice = {cls, paymentMessage(p, plate)};
    return r;
}

bool resetKasse(KasseLink& kasse)
{
    return kasse.postJson("/api/reset", "{}").status == 200;
}
