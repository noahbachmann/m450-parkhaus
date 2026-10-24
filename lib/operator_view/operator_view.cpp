#include "operator_view.h"

#include <ArduinoJson.h>

#include "format.h"
#include "plate.h"
#include "schranken_view.h"

const char* const NEW_CUSTOMER_REASON = "Neuer Kunde registrieren";

namespace
{
    struct KasseOutcome
    {
        bool confirmed = false; // Pico 1 answered as expected: the barrier may open
        std::string note;       // log note / notice text
    };

    KasseOutcome entryOutcome(const HttpReply& reply)
    {
        if (reply.status == 201)
        {
            return {true, "Parkvorgang eröffnet"};
        }
        if (reply.status == 409)
        {
            return {true, "Parkvorgang bereits offen"};
        }
        return {false, ""};
    }

    // Override exit reply: which way the session was closed.
    KasseOutcome exitOutcome(const HttpReply& reply)
    {
        if (reply.status == 404)
        {
            return {true, "kein Parkvorgang"};
        }
        JsonDocument doc;
        if (reply.status != 200 || deserializeJson(doc, reply.body) != DeserializationError::Ok ||
            !doc["status"].is<const char*>())
        {
            return {false, ""};
        }
        std::string status = doc["status"].as<std::string>();
        if (status == "closed_unpaid")
        {
            return {true, "ohne Zahlung"};
        }
        if (status == "closed")
        {
            return {true, "bezahlt"};
        }
        if (status == "closed_free")
        {
            return {true, "Freizeit"};
        }
        return {false, ""};
    }

    // «Kennzeichen registriert: NW654321 – » after a new-customer registration.
    std::string registeredPrefix(const RemoteOpenResult& r, const std::string& plate)
    {
        return r.registered ? registrationMessage(RegistrationResult::Ok) + ": " + plate + " – " : "";
    }

    std::string kzBody(const std::string& plate, bool override)
    {
        JsonDocument doc;
        doc["kz"] = plate;
        if (override)
        {
            doc["override"] = true;
        }
        std::string body;
        serializeJson(doc, body);
        return body;
    }

    void appendLane(std::string& html, const OperatorLane& l)
    {
        html += "<div class=\"card\"><h2>";
        html += gateLaneLabel(l.lane);
        html += "</h2>";
        if (!l.fault.empty())
        {
            appendNotice(html, "err", l.fault);
        }
        if (!l.call.empty())
        {
            appendNotice(html, "info", l.call);
        }
        html += "<table>";
        appendRow(html, "Schranke", l.barrier);
        appendRow(html, "Letztes Kennzeichen", l.lastPlate.empty() ? "–" : l.lastPlate);
        appendRow(html, "Status", l.status.empty() ? "–" : l.status);
        html += "</table><details><summary><strong>Schranke öffnen</strong></summary>"
                "<form method=\"post\" action=\"/betrieb/oeffnen\"><input type=\"hidden\" name=\"lane\" value=\"";
        html += gateLaneName(l.lane);
        html += "\"><p><label>Kennzeichen <input name=\"kz\" value=\"" + htmlEscape(l.lastPlate) +
                "\" autocomplete=\"off\"></label></p><p><label>Grund <select name=\"reason\">"
                "<option value=\"\">– bitte wählen –</option>";
        for (const std::string& reason : remoteOpenReasons())
        {
            html += "<option>" + htmlEscape(reason) + "</option>";
        }
        if (l.lane == GateLane::Entry)
        {
            html += "<option>";
            html += NEW_CUSTOMER_REASON;
            html += "</option></select></label></p><p><label>Name (nur für «";
            html += NEW_CUSTOMER_REASON;
            html += "») <input name=\"name\" maxlength=\"40\" autocomplete=\"off\"></label></p>";
        }
        else
        {
            html += "</select></label></p>";
        }
        html += "<button type=\"submit\">Bestätigen</button></form></details></div>";
    }

    void appendLog(std::string& html, const EventLog& log, const std::string& filter)
    {
        EventType wanted = EventType::SafetyStop;
        bool filtered = eventTypeFromName(filter, wanted);
        html += "<h2>Ereignisprotokoll</h2><form method=\"get\" action=\"/betrieb\"><label>Ereignis "
                "<select name=\"filter\"><option value=\"\">alle</option>";
        size_t typeCount = 0;
        const EventType* types = allEventTypes(typeCount);
        for (size_t i = 0; i < typeCount; ++i)
        {
            EventType t = types[i];
            html += "<option value=\"";
            html += eventTypeName(t);
            html += filtered && t == wanted ? "\" selected>" : "\">";
            html += eventTypeLabel(t);
            html += "</option>";
        }
        html += "</select></label> <button type=\"submit\">Filtern</button></form>";

        std::string rows;
        size_t count = 0;
        const std::vector<Event>& all = log.all();
        for (auto it = all.rbegin(); it != all.rend(); ++it)
        {
            if (filtered && it->type != wanted)
            {
                continue;
            }
            count++;
            rows += "<tr><td>" + formatDateTime(it->time) + "</td><td>" + eventTypeLabel(it->type) + "</td><td>" +
                    gateLaneLabel(it->lane) + "</td><td>" + htmlEscape(it->plate) + "</td><td>" +
                    htmlEscape(it->user) + "</td><td>" + htmlEscape(it->reason) + "</td><td>" +
                    htmlEscape(it->note) + "</td></tr>";
        }
        html += "<p class=\"muted\">" + std::to_string(count) + (count == 1 ? " Eintrag" : " Einträge") + "</p>";
        if (count > 0)
        {
            html += "<div class=\"scroll\"><table><tr><th>Zeit</th><th>Ereignis</th><th>Spur</th><th>Kennzeichen</th>"
                    "<th>Benutzer</th><th>Grund</th><th>Bemerkung</th></tr>" +
                    rows + "</table></div>";
        }
    }
}

const std::vector<std::string>& remoteOpenReasons()
{
    static const std::vector<std::string> reasons = {"Kennzeichen nicht erkannt", "Kulanz", "Technische Störung"};
    return reasons;
}

std::string laneStatus(KasseLink& kasse, const std::string& plate)
{
    if (!isValidPlate(plate))
    {
        return "–";
    }
    HttpReply reply = kasse.getJson("/api/status?kz=" + plate);
    if (reply.status <= 0)
    {
        return "Kasse nicht erreichbar";
    }
    JsonDocument doc;
    if (reply.status != 200 || deserializeJson(doc, reply.body) != DeserializationError::Ok ||
        !doc["status"].is<const char*>())
    {
        return "unbekannte Antwort der Kasse";
    }
    std::string status = doc["status"].as<std::string>();
    if (status == "unbekannt")
    {
        return "kein Parkvorgang";
    }
    if (status == "unbezahlt" && doc["fee"].is<uint32_t>())
    {
        return "unbezahlt (" + formatChf(doc["fee"].as<uint32_t>()) + ")";
    }
    return status;
}

RemoteOpenResult remoteOpen(GateLane lane, const std::string& rawPlate, const std::string& reason,
                            const std::string& name, int64_t now, KasseLink& kasse, RegistrationList& registrations,
                            EventLog& log)
{
    RemoteOpenResult r;
    bool newCustomer = reason == NEW_CUSTOMER_REASON;
    bool knownReason = newCustomer;
    for (const std::string& allowed : remoteOpenReasons())
    {
        knownReason = knownReason || reason == allowed;
    }
    if (!knownReason)
    {
        r.notice = {"err", "Bitte Grund auswählen"};
        return r;
    }
    if (lane == GateLane::None)
    {
        r.notice = {"err", "Ungültige Spur"};
        return r;
    }

    std::string plate = normalizePlate(rawPlate);
    if (!isValidPlate(plate))
    {
        r.notice = {"err", "Bitte gültiges Kennzeichen eingeben"};
        return r;
    }
    if (newCustomer)
    {
        if (lane != GateLane::Entry)
        {
            r.notice = {"err", "Neukunden nur an der Einfahrt registrieren"};
            return r;
        }
        RegistrationResult added = registrations.add(name, plate);
        if (added != RegistrationResult::Ok)
        {
            r.notice = {"err", registrationMessage(added) + " – Schranke bleibt zu"};
            return r;
        }
        r.registered = true;
    }

    HttpReply reply = lane == GateLane::Entry ? kasse.postJson("/api/entry", kzBody(plate, false))
                                              : kasse.postJson("/api/exit", kzBody(plate, true));
    if (reply.status <= 0)
    {
        Event e;
        e.time = now;
        e.type = EventType::KasseUnreachable;
        e.lane = lane;
        e.plate = plate;
        e.user = OPERATOR_USER;
        e.reason = reason;
        e.note = "Fernöffnung abgebrochen, Schranke bleibt zu";
        log.add(e);
        r.logged = true;
        r.notice = {"err", registeredPrefix(r, plate) + "Kasse nicht erreichbar – Schranke bleibt zu"};
        return r;
    }
    KasseOutcome outcome = lane == GateLane::Entry ? entryOutcome(reply) : exitOutcome(reply);
    if (!outcome.confirmed)
    {
        r.notice = {"err", registeredPrefix(r, plate) + "Unerwartete Antwort der Kasse (" +
                               std::to_string(reply.status) + ") – Schranke bleibt zu"};
        return r;
    }
    if (newCustomer)
    {
        outcome.note = "Neukunde registriert – " + outcome.note;
    }

    Event e;
    e.time = now;
    e.type = EventType::RemoteOpening;
    e.lane = lane;
    e.plate = plate;
    e.user = OPERATOR_USER;
    e.reason = reason;
    e.note = outcome.note;
    log.add(e);

    r.open = true;
    r.plate = plate;
    r.logged = true;
    r.notice = {"ok", std::string("Schranke ") + gateLaneLabel(lane) + " geöffnet – " + outcome.note};
    return r;
}

bool logGateResult(EventLog& log, GateLane lane, const GateResult& result, int64_t now)
{
    if (!result.unreachable)
    {
        return false;
    }
    Event e;
    e.time = now;
    e.type = EventType::KasseUnreachable;
    e.lane = lane;
    e.plate = result.plate;
    e.note = result.message;
    log.add(e);
    return true;
}

bool logBarrierEvent(EventLog& log, GateLane lane, BarrierEvent event, int64_t now)
{
    if (event == BarrierEvent::None)
    {
        return false;
    }
    Event e;
    e.time = now;
    e.lane = lane;
    if (event == BarrierEvent::SafetyStop)
    {
        e.type = EventType::SafetyStop;
        e.note = "Schliessen gestoppt, Schranke geöffnet";
    }
    else
    {
        e.type = EventType::LoopFault;
        e.note = loopFaultMessage(gateLaneLabel(lane));
    }
    log.add(e);
    return true;
}

std::string renderOperatorPage(const OperatorLane& entry, const OperatorLane& exit, const EventLog& log,
                               const std::string& filter, const Notice& notice)
{
    std::string html;
    html.reserve(8192);
    appendPageStart(html, "Betrieb", Role::Admin, "Betrieb – Leitstelle");
    if (notice.cls[0] != '\0')
    {
        appendNotice(html, notice.cls, notice.text);
    }
    html += "<p class=\"muted\"><a href=\"/betrieb\">Aktualisieren</a></p><div class=\"lanes\">";
    appendLane(html, entry);
    appendLane(html, exit);
    html += "</div>";
    appendLog(html, log, filter);
    html += "<h2>Testdaten</h2><form method=\"post\" action=\"/reset\" "
            "onsubmit=\"return confirm('Registrierungen, Ereignisprotokoll und alle Parkvorgänge zurücksetzen?')\">"
            "<button type=\"submit\">Testdaten zurücksetzen</button></form><p class=\"muted\">Registrierungen KZ-01, "
            "KZ-02, KZ-08, KZ-10; Ereignisprotokoll leer; Kasse: keine Parkvorgänge, Transaktionszähler auf 1.</p>";
    appendPageEnd(html);
    return html;
}
