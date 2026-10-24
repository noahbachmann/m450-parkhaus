#include "terminal.h"

#include <ArduinoJson.h>

#include "format.h"

namespace
{
    const char* DECLINED = "Zahlung abgelehnt – anderes Zahlungsmittel wählen";
    const char* NOTIFIED = "Zahlung abgelehnt – Leitstelle wurde benachrichtigt";

    GateResult message(const std::string& plate, const char* cls, const std::string& text)
    {
        GateResult r;
        r.cls = cls;
        r.plate = plate;
        r.message = text;
        return r;
    }

    void clearPayment(LaneTerminal& t)
    {
        t.payPlate.clear();
        t.payFeeRappen = 0;
        t.declines = 0;
    }

    void call(LaneTerminal& t, int64_t now, const char* text)
    {
        t.callPending = true;
        t.callTime = now;
        t.callText = text;
    }

    std::string errorCode(const HttpReply& reply)
    {
        JsonDocument doc;
        if (deserializeJson(doc, reply.body) != DeserializationError::Ok || !doc["error"].is<const char*>())
        {
            return "";
        }
        return doc["error"].as<std::string>();
    }

    // "Bezahlt – CHF 6.00, TWINT"; empty if the reply isn't a valid payment confirmation for the plate.
    std::string paidText(const HttpReply& reply, const std::string& plate)
    {
        JsonDocument doc;
        if (deserializeJson(doc, reply.body) != DeserializationError::Ok || !doc["kz"].is<const char*>() ||
            doc["kz"].as<std::string>() != plate || !doc["amount"].is<uint32_t>() ||
            !doc["method"].is<const char*>())
        {
            return "";
        }
        std::string method = doc["method"].as<std::string>() == "twint" ? "TWINT" : "Karte";
        return "Bezahlt – " + formatChf(doc["amount"].as<uint32_t>()) + ", " + method;
    }
}

void recordGate(LaneTerminal& t, const GateResult& result)
{
    t.lastPlate = result.plate;
    if (result.open)
    {
        clearPayment(t);
        t.callPending = false;
        t.passing = false;
        return;
    }
    if (result.unpaid)
    {
        if (result.plate != t.payPlate)
        {
            t.declines = 0;
        }
        t.payPlate = result.plate;
        t.payFeeRappen = result.feeRappen;
        return;
    }
    clearPayment(t);
}

void recordRemoteOpening(LaneTerminal& t, const std::string& plate)
{
    clearPayment(t);
    t.callPending = false;
    t.passing = false;
    t.lastPlate = plate;
}

GateResult driveThrough(LaneTerminal& t, bool barrierUp)
{
    if (!barrierUp)
    {
        return message(t.lastPlate, "err", "Keine offene Schranke – Fahrzeug kann nicht durchfahren");
    }
    t.passing = true;
    std::string who = t.lastPlate.empty() ? "Fahrzeug" : t.lastPlate;
    return message(t.lastPlate, "ok", who + " durchgefahren – Schranke schliesst");
}

GateResult leftLoop(LaneTerminal& t, bool barrierUp)
{
    if (!barrierUp || t.passing)
    {
        return message(t.lastPlate, "", "");
    }
    return driveThrough(t, true);
}

GateResult closedAfterPass(LaneTerminal& t)
{
    if (!t.passing)
    {
        return message(t.lastPlate, "", "");
    }
    t.passing = false;
    std::string who = t.lastPlate.empty() ? "Fahrzeug" : t.lastPlate;
    return message(t.lastPlate, "ok", who + " durchgefahren – Schranke geschlossen");
}

GateResult pressCallButton(LaneTerminal& t, int64_t now, EventLog& log)
{
    call(t, now, "Ruftaste gedrückt");
    Event e;
    e.time = now;
    e.type = EventType::CallButton;
    e.lane = t.lane;
    e.plate = t.lastPlate;
    log.add(e);
    return message(t.lastPlate, "ok", "Leitstelle wurde gerufen – bitte warten");
}

GateResult abortAtTerminal(LaneTerminal& t)
{
    std::string plate = t.payPlate.empty() ? t.lastPlate : t.payPlate;
    clearPayment(t);
    return message(plate, "ok", "Vorgang abgebrochen – Fahrzeug wendet");
}

GateResult payAtTerminal(LaneTerminal& t, const std::string& method, int64_t now, KasseLink& kasse, EventLog& log)
{
    if (t.payPlate.empty())
    {
        return message("", "err", "Kein Zahlvorgang – bitte Kennzeichen an der Ausfahrt erfassen");
    }
    if (method != "card_ok" && method != "card_declined" && method != "twint")
    {
        return message(t.payPlate, "err", "Bitte Zahlungsmittel wählen");
    }
    std::string plate = t.payPlate;
    JsonDocument req;
    req["kz"] = plate;
    req["method"] = method;
    std::string body;
    serializeJson(req, body);
    HttpReply reply = kasse.postJson("/api/pay", body);

    if (reply.status <= 0)
    {
        GateResult r = message(plate, "err", "Kasse nicht erreichbar – bitte Ruftaste drücken");
        r.unreachable = true;
        return r;
    }
    std::string paid;
    std::string code = errorCode(reply);
    if (reply.status == 402 && code == "declined")
    {
        t.declines++;
        if (t.declines == MAX_DECLINES)
        {
            call(t, now, "Zahlung 5× abgelehnt");
            Event e;
            e.time = now;
            e.type = EventType::PaymentAlert;
            e.lane = t.lane;
            e.plate = plate;
            e.note = std::to_string(MAX_DECLINES) + " Zahlungsversuche abgelehnt";
            log.add(e);
        }
        return message(plate, "err", t.declines >= MAX_DECLINES ? NOTIFIED : DECLINED);
    }
    if (reply.status == 200)
    {
        paid = paidText(reply, plate);
        if (paid.empty())
        {
            return message(plate, "err", "Unerwartete Antwort der Kasse – bitte Ruftaste drücken");
        }
    }
    else if (!(reply.status == 409 && (code == "already_paid" || code == "nothing_to_pay")))
    {
        return message(plate, "err", "Zahlung nicht möglich – bitte Ruftaste drücken");
    }

    GateResult exit = exitRequest(plate, kasse);
    recordGate(t, exit);
    if (!paid.empty())
    {
        exit.message = paid + " – " + exit.message;
    }
    return exit;
}

std::string callLabel(const LaneTerminal& t)
{
    if (!t.callPending)
    {
        return "";
    }
    std::string text = t.callText;
    if (!t.lastPlate.empty())
    {
        text += " – " + t.lastPlate;
    }
    return text + " (" + formatDateTime(t.callTime) + ")";
}
