#include "schranken_view.h"

#include "plate.h"

const char* const SCHRANKEN_NAV = "<a href=\"/kamera\">Kamerasimulation</a>"
                                  "<a href=\"/registrierung\">Registrierung</a><a href=\"/betrieb\">Betrieb</a>";

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
        html += "\"><p><label>Kennzeichen <input name=\"kz\" placeholder=\"LU 123456\" autocomplete=\"off\"></label></p>";
        if (withEntryTime)
        {
            html += "<p><label>Einfahrtszeit (leer = jetzt) <input type=\"datetime-local\" name=\"entry\" step=\"1\">"
                    "</label></p>";
        }
        html += "<button type=\"submit\">Erfassen</button></form>";
        if (!withEntryTime)
        {
            appendExitTerminal(html, lane);
        }
        html += "<form method=\"post\" action=\"/kamera/ruftaste\" class=\"inline\"><input type=\"hidden\" "
                "name=\"lane\" value=\"";
        html += withEntryTime ? "entry" : "exit";
        html += "\"><button type=\"submit\">Ruftaste</button></form></div>";
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
    appendPageStart(html, "Registrierung", SCHRANKEN_NAV, "Registrierung");
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
    html += "<h2>Testdaten</h2>"
            "<form method=\"post\" action=\"/reset\" "
            "onsubmit=\"return confirm('Registrierungen auf die Testdaten zurücksetzen?')\">"
            "<button type=\"submit\">Testdaten zurücksetzen</button></form>";
    appendPageEnd(html);
    return html;
}

std::string renderCameraPage(const LaneView& entry, const LaneView& exit)
{
    std::string html;
    html.reserve(4096);
    appendPageStart(html, "Kamerasimulation", SCHRANKEN_NAV, "Kamerasimulation");
    html += "<div class=\"lanes\">";
    appendLane(html, "Einfahrt", "/kamera/einfahrt", true, entry);
    appendLane(html, "Ausfahrt", "/kamera/ausfahrt", false, exit);
    html += "</div><p class=\"muted\"><a href=\"/kamera\">Aktualisieren</a> – die Schranke schliesst frühestens nach 5 s, "
            "sobald die Sicherungsschleife frei ist.</p>";
    appendPageEnd(html);
    return html;
}
