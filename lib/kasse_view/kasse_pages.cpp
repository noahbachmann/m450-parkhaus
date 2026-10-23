#include "kasse_pages.h"

#include "format.h"
#include "kasse_view.h"
#include "plate.h"

namespace
{
void appendSession(std::string& html, const SessionView& v)
{
    html += "<div class=\"card\"><table>";
    appendRow(html, "Kennzeichen", v.plate);
    appendRow(html, "Einfahrt", v.entry);
    appendRow(html, "Ausfahrt", v.exit);
    appendRow(html, "Parkdauer", v.duration);
    appendRow(html, "Gebühr", v.fee);
    appendRow(html, "Tarif", v.tariffNote);
    appendRow(html, "Bargeld eingeworfen", v.cashInserted);
    appendRow(html, "Zahlungsstatus", v.paymentStatus);
    appendRow(html, "Status", v.status);
    appendRow(html, "Zahlung", v.payment);
    html += "</table></div>";
}

void appendPayForm(std::string& html, Page page, const std::string& plate)
{
    html += "<form method=\"post\" action=\"";
    html += pagePath(page);
    html += "/pay\" class=\"card methods\"><input type=\"hidden\" name=\"kz\" value=\"" + htmlEscape(plate) + "\">"
            "<strong>Zahlungsmittel</strong>"
            "<label><input type=\"radio\" name=\"method\" value=\"card_ok\" checked> Karte OK</label>"
            "<label><input type=\"radio\" name=\"method\" value=\"card_declined\"> Karte abgelehnt</label>"
            "<label><input type=\"radio\" name=\"method\" value=\"twint\"> TWINT</label>"
            "<label><input type=\"radio\" name=\"method\" value=\"cash\"> Bargeld</label>"
            "<label>Eingeworfener Betrag in CHF (nur Bargeld): "
            "<input name=\"amount\" inputmode=\"decimal\" placeholder=\"10.00\" size=\"8\"></label>"
            "<button type=\"submit\">Bezahlen</button></form>";
}

void appendSessionList(std::string& html, const SessionStore& store, int64_t now)
{
    html += "<h2>Parkvorgänge</h2>";
    const auto& all = store.all();
    if (all.empty())
    {
        html += "<p class=\"muted\">Keine Parkvorgänge gespeichert.</p>";
        return;
    }
    html += "<div class=\"scroll\"><table><tr><th>Kennzeichen</th><th>Einfahrt</th><th>Ausfahrt</th><th>Dauer</th>"
            "<th>Gebühr</th><th>Status</th><th>Zahlung</th></tr>";
    for (auto it = all.rbegin(); it != all.rend(); ++it)
    {
        SessionView v = describeSession(*it, now);
        html += "<tr><td><a href=\"/kasse?kz=" + htmlEscape(v.plate) + "\">" + htmlEscape(v.plate) + "</a></td><td>" +
                htmlEscape(v.entry) + "</td><td>" + htmlEscape(v.exit) + "</td><td>" + htmlEscape(v.duration) +
                "</td><td>" + htmlEscape(v.fee) + "</td><td>" + htmlEscape(v.status) + "</td><td>" +
                htmlEscape(v.payment) + "</td></tr>";
    }
    html += "</table></div>";
}

void appendResetForm(std::string& html)
{
    html += "<h2>Testdaten</h2>"
            "<form method=\"post\" action=\"/reset\" "
            "onsubmit=\"return confirm('Alle Parkvorgänge löschen und Transaktionszähler zurücksetzen?')\">"
            "<button type=\"submit\">Testdaten zurücksetzen</button></form>";
}
}

const char* pagePath(Page page)
{
    return page == Page::Kasse ? "/kasse" : "/automat";
}

std::string renderPage(Page page, const SessionStore& store, const std::string& query, const Notice& notice,
                       int64_t now, bool timeOk)
{
    bool kasse = page == Page::Kasse;
    std::string html;
    html.reserve(4096);
    appendPageStart(html, kasse ? "Kasse" : "Automat", "<a href=\"/kasse\">Kasse</a><a href=\"/automat\">Automat</a>",
                    kasse ? "Kasse" : "Automat – Parkgebühr bezahlen");

    if (!timeOk)
    {
        appendNotice(html, "err", "Uhrzeit nicht synchronisiert – Gebühren und Zahlungen nicht möglich");
    }
    if (notice.cls[0] != '\0')
    {
        appendNotice(html, notice.cls, notice.text);
    }

    html += "<form method=\"get\" action=\"";
    html += pagePath(page);
    html += "\"><label>Kennzeichen <input name=\"kz\" value=\"" + htmlEscape(query) +
            "\" placeholder=\"LU 123456\" autocomplete=\"off\" autofocus></label> "
            "<button type=\"submit\">Suchen</button></form>";

    std::string plate = normalizePlate(query);
    if (!plate.empty())
    {
        const ParkingSession* s = store.findActive(plate);
        if (s == nullptr && kasse)
        {
            s = store.findLatest(plate);
        }
        if (!isValidPlate(plate))
        {
            appendNotice(html, "err", "Ungültiges Kennzeichen – erwartet 2 Buchstaben und 1–6 Ziffern");
        }
        else if (s == nullptr)
        {
            std::string text = noSessionMessage(plate);
            if (text != notice.text)
            {
                appendNotice(html, "err", text);
            }
        }
        else
        {
            SessionView v = describeSession(*s, now);
            appendSession(html, v);
            // Also offered when paid, so a second attempt answers «bereits bezahlt» (TF-20).
            if (timeOk && (v.payable || s->status == SessionStatus::Paid))
            {
                appendPayForm(html, page, v.plate);
            }
        }
    }

    if (kasse)
    {
        appendSessionList(html, store, now);
        appendResetForm(html);
    }
    if (timeOk)
    {
        html += "<p class=\"muted\">Stand: " + formatDateTime(now) + "</p>";
    }
    appendPageEnd(html);
    return html;
}
