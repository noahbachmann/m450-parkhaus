#include "schranken_view.h"

#include <set>

#include "kasse_view.h"
#include "plate.h"

namespace
{

    void appendExitTerminal(std::string& html, const LaneView& lane)
    {
        if (!lane.payPlate.empty())
        {
            html += "<form method=\"post\" action=\"/kamera/bezahlen\" class=\"methods\"><p><strong>Gebühr " +
                    htmlEscape(lane.payFee) + " für " + htmlEscape(lane.payPlate) +
                    " – hier bezahlen</strong></p>"
                    "<label><input type=\"radio\" name=\"method\" value=\"card_ok\" checked> Karte OK</label>"
                    "<label><input type=\"radio\" name=\"method\" value=\"card_declined\"> Karte abgelehnt</label>"
                    "<label><input type=\"radio\" name=\"method\" value=\"twint\"> TWINT</label>"
                    "<button type=\"submit\">Bezahlen</button></form>";
        }
        html += "<form method=\"post\" action=\"/kamera/abbrechen\" class=\"inline\">"
                "<button type=\"submit\">Fahrzeug wendet</button></form> ";
    }

    void appendLane(std::string& html, const char* title, const char* action, bool withEntryTime, const LaneView& lane)
    {
        html += "<div class=\"card\"><h2>";
        html += title;
        html += "</h2>";
        if (!lane.fault.empty())
        {
            appendNotice(html, "err", lane.fault);
        }
        html += "<p>Schranke: <strong>";
        html += lane.barrier;
        html += "</strong></p><div class=\"terminal ";
        html += lane.cls;
        html += "\">" + htmlEscape(lane.terminal) + "</div><form method=\"post\" action=\"";
        html += action;
        html += "\"><p><label>Kennzeichen <input name=\"kz\" placeholder=\"LU 123456\" autocomplete=\"off\" "
                "list=\"plates\"></label></p>";
        if (withEntryTime)
        {
            html += "<p><label>Einfahrtszeit (leer = jetzt) <input type=\"datetime-local\" name=\"entry\" step=\"1\">"
                    "</label></p>";
        }
        html += "<button type=\"submit\">Erfassen</button></form>";
        const char* laneName = withEntryTime ? "entry" : "exit";
        if (lane.canPass)
        {
            html += "<form method=\"post\" action=\"/kamera/durchfahren\" class=\"inline\"><input type=\"hidden\" "
                    "name=\"lane\" value=\"";
            html += laneName;
            html += "\"><button type=\"submit\"><strong>Durchfahren</strong></button></form> ";
        }
        if (!withEntryTime)
        {
            appendExitTerminal(html, lane);
        }
        html += "<form method=\"post\" action=\"/kamera/ruftaste\" class=\"inline\"><input type=\"hidden\" "
                "name=\"lane\" value=\"";
        html += laneName;
        html += "\"><button type=\"submit\">Ruftaste</button></form></div>";
    }

    void appendPlateSuggestions(std::string& html, const KasseSnapshot& kasse, const RegistrationList& registrations)
    {
        std::set<std::string> plates;
        for (const Registration& r : registrations.all())
        {
            plates.insert(r.plate);
        }
        for (const ParkingSession& s : kasse.sessions.all())
        {
            if (isActive(s.status))
            {
                plates.insert(s.plate);
            }
        }
        html += "<datalist id=\"plates\">";
        for (const std::string& plate : plates)
        {
            html += "<option value=\"" + htmlEscape(plate) + "\">";
        }
        html += "</datalist>";
    }

    void appendSessions(std::string& html, const KasseSnapshot& kasse)
    {
        html += "<h2>Parkvorgänge (Kasse)</h2>";
        if (!kasse.reachable)
        {
            html += "<p class=\"muted\">Kasse nicht erreichbar – Parkvorgänge können nicht angezeigt werden.</p>";
            return;
        }
        if (kasse.sessions.all().empty())
        {
            html += "<p class=\"muted\">Keine Parkvorgänge.</p>";
            return;
        }
        html += "<p class=\"muted\">Offene zuerst, dann abgeschlossene – zum Testen.</p><div class=\"scroll\"><table>"
                "<tr><th>Kennzeichen</th><th>Einfahrt</th><th>Ausfahrt</th><th>Dauer</th><th>Gebühr</th>"
                "<th>Status</th></tr>";
        for (const ParkingSession* s : sessionsForList(kasse.sessions))
        {
            SessionView v = describeSession(*s, kasse.now);
            std::string status = v.status;
            if (s->status == SessionStatus::Open)
            {
                status += " (unbezahlt)";
            }
            html += "<tr><td>" + htmlEscape(v.plate) + "</td><td>" + htmlEscape(v.entry) + "</td><td>" +
                    htmlEscape(v.exit) + "</td><td>" + htmlEscape(v.duration) + "</td><td>" + htmlEscape(v.fee) +
                    "</td><td>" + htmlEscape(status) + "</td></tr>";
        }
        html += "</table></div>";
    }
}

std::string registrationMessage(RegistrationResult result)
{
    switch (result)
    {
    case RegistrationResult::Ok:
        return "Kennzeichen registriert";
    case RegistrationResult::Duplicate:
        return "Kennzeichen bereits registriert";
    case RegistrationResult::InvalidPlate:
        return "Ungültiges Kennzeichen – erwartet 2 Buchstaben und 1–6 Ziffern";
    case RegistrationResult::InvalidName:
        return "Bitte Namen eingeben (1–" + std::to_string(MAX_NAME_BYTES) + " Zeichen)";
    case RegistrationResult::Full:
        return "Maximal " + std::to_string(MAX_REGISTRATIONS) + " Registrierungen – bitte zuerst eine löschen";
    }
    return "";
}

FormResult handleRegistrationForm(RegistrationList& list, const std::string& name, const std::string& kz)
{
    RegistrationResult r = list.add(name, kz);
    FormResult out;
    out.changed = r == RegistrationResult::Ok;
    out.notice = {out.changed ? "ok" : "err", registrationMessage(r)};
    if (out.changed)
    {
        out.notice.text += ": " + normalizePlate(kz);
    }
    return out;
}

FormResult handleDeleteForm(RegistrationList& list, const std::string& kz)
{
    std::string plate = normalizePlate(kz);
    FormResult out;
    out.changed = list.remove(plate);
    if (out.changed)
    {
        out.notice = {"ok", "Registrierung " + plate + " gelöscht"};
    }
    else
    {
        out.notice = {"err", "Kennzeichen " + plate + " nicht registriert"};
    }
    return out;
}

std::string renderRegistrationPage(const RegistrationList& list, const Notice& notice)
{
    std::string html;
    html.reserve(4096);
    appendPageStart(html, "Registrierung", Role::Admin, "Registrierung");
    if (notice.cls[0] != '\0')
    {
        appendNotice(html, notice.cls, notice.text);
    }
    html += "<form method=\"post\" action=\"/registrierung/add\" class=\"card\">"
            "<p><label>Name <input name=\"name\" maxlength=\"40\" autocomplete=\"off\"></label></p>"
            "<p><label>Kennzeichen <input name=\"kz\" placeholder=\"LU 123456\" autocomplete=\"off\"></label></p>"
            "<button type=\"submit\">Registrieren</button></form>";

    html += "<h2>Registrierte Kennzeichen</h2>";
    if (list.all().empty())
    {
        html += "<p class=\"muted\">Keine Kennzeichen registriert.</p>";
    }
    else
    {
        html += "<div class=\"scroll\"><table><tr><th>Kennzeichen</th><th>Name</th><th></th></tr>";
        for (const Registration& r : list.all())
        {
            html += "<tr><td>" + htmlEscape(r.plate) + "</td><td>" + htmlEscape(r.name) +
                    "</td><td><form method=\"post\" action=\"/registrierung/delete\" class=\"inline\">"
                    "<input type=\"hidden\" name=\"kz\" value=\"" +
                    htmlEscape(r.plate) + "\"><button type=\"submit\">Löschen</button></form></td></tr>";
        }
        html += "</table></div>";
    }
    html += "<p class=\"muted\">Testdaten zurücksetzen: auf der Seite <a href=\"/betrieb\">Betrieb</a>.</p>";
    appendPageEnd(html);
    return html;
}

std::string renderLoginPage(const Notice& notice, const std::string& next)
{
    std::string html;
    html.reserve(2048);
    appendPageStart(html, "Anmelden", Role::Public, "Anmelden – Betriebspersonal");
    if (notice.cls[0] != '\0')
    {
        appendNotice(html, notice.cls, notice.text);
    }
    html += "<form method=\"post\" action=\"/login\" class=\"card\"><input type=\"hidden\" name=\"next\" value=\"" +
            htmlEscape(next) +
            "\"><p><label>Benutzer <input name=\"user\" autocomplete=\"username\" autofocus></label></p>"
            "<p><label>Passwort <input type=\"password\" name=\"password\" autocomplete=\"current-password\">"
            "</label></p><button type=\"submit\">Anmelden</button></form>"
            "<p class=\"muted\">Kasse, Registrierung und Betrieb sind nur für das Betriebspersonal.</p>";
    appendPageEnd(html);
    return html;
}

std::string renderCameraPage(const LaneView& entry, const LaneView& exit, const KasseSnapshot& kasse,
                             const RegistrationList& registrations, Role role)
{
    std::string html;
    html.reserve(8192);
    appendPageStart(html, "Kamerasimulation", role, "Kamerasimulation");
    appendPlateSuggestions(html, kasse, registrations);
    html += "<div class=\"lanes\">";
    appendLane(html, "Einfahrt", "/kamera/einfahrt", true, entry);
    appendLane(html, "Ausfahrt", "/kamera/ausfahrt", false, exit);
    html += "</div><p class=\"muted\"><a href=\"/kamera\">Aktualisieren</a> – die Schranke schliesst, sobald das "
            "Fahrzeug durchgefahren ist («Durchfahren»), frühestens 5 s nach dem Öffnen.</p>";
    appendSessions(html, kasse);
    appendPageEnd(html);
    return html;
}
