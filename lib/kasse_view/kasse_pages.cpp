#include "kasse_pages.h"

#include "format.h"
#include "kasse_view.h"
#include "plate.h"
#include "tariff.h"

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

void appendPayForm(std::string& html, const std::string& plate, const char* action)
{
    html += "<form method=\"post\" action=\"";
    html += action;
    html += "\" class=\"card methods\"><input type=\"hidden\" name=\"kz\" value=\"" + htmlEscape(plate) + "\">"
            "<strong>Zahlungsmittel</strong>";
    appendPaymentMethods(html, true);
    html += "<label>Eingeworfener Betrag in CHF (nur Bargeld): "
            "<input name=\"amount\" inputmode=\"decimal\" placeholder=\"10.00\" size=\"8\"></label>"
            "<button type=\"submit\">Bezahlen</button></form>";
}

void appendSessionList(std::string& html, const SessionStore& store, int64_t now)
{
    html += "<h2>Parkvorgänge</h2>";
    if (store.all().empty())
    {
        html += "<p class=\"muted\">Keine Parkvorgänge gespeichert.</p>";
        return;
    }
    appendSessionTable(html, store, now, true);
}
}

void appendPaymentMethods(std::string& html, bool withCash)
{
    html += "<label><input type=\"radio\" name=\"method\" value=\"card_ok\" checked> Karte OK</label>"
            "<label><input type=\"radio\" name=\"method\" value=\"card_declined\"> Karte abgelehnt</label>"
            "<label><input type=\"radio\" name=\"method\" value=\"twint\"> TWINT</label>";
    if (withCash)
    {
        html += "<label><input type=\"radio\" name=\"method\" value=\"cash\"> Bargeld</label>";
    }
}

void appendSessionTable(std::string& html, const SessionStore& store, int64_t now, bool kasseLinks)
{
    html += "<div class=\"scroll\"><table><tr><th>Kennzeichen</th><th>Einfahrt</th><th>Ausfahrt</th><th>Dauer</th>"
            "<th>Gebühr</th><th>Status</th>";
    html += kasseLinks ? "<th>Zahlung</th></tr>" : "</tr>";
    for (const ParkingSession* s : sessionsForList(store))
    {
        SessionView v = describeSession(*s, now);
        std::string plate = htmlEscape(v.plate);
        std::string status = v.status;
        if (kasseLinks)
        {
            plate = "<a href=\"/kasse?kz=" + plate + "\">" + plate + "</a>";
        }
        else if (s->status == SessionStatus::Open)
        {
            status += " (unbezahlt)";
        }
        html += "<tr><td>" + plate + "</td><td>" + htmlEscape(v.entry) + "</td><td>" + htmlEscape(v.exit) +
                "</td><td>" + htmlEscape(v.duration) + "</td><td>" + htmlEscape(v.fee) + "</td><td>" +
                htmlEscape(status) + "</td>";
        html += kasseLinks ? "<td>" + htmlEscape(v.payment) + "</td></tr>" : "</tr>";
    }
    html += "</table></div>";
}

namespace
{
// "1 h 0 min 1 s": like formatDuration, plus seconds where they matter for the boundary.
std::string exampleDuration(int64_t seconds)
{
    std::string text = formatDuration(seconds);
    if (seconds % 60 != 0)
    {
        text += " " + std::to_string(seconds % 60) + " s";
    }
    return text;
}
}

std::string renderTariffPage(Role role)
{
    const int64_t MIN = 60;
    const int64_t HOUR = 3600;
    const int64_t examples[] = {
        FREE_MINUTES * MIN,
        FREE_MINUTES * MIN + 1,
        HOUR,
        HOUR + 1,
        HOUR + 20 * MIN,
        2 * HOUR + 30 * MIN,
        10 * HOUR,
        10 * HOUR + 1,
        DAY_BLOCK_HOURS * HOUR,
        DAY_BLOCK_HOURS * HOUR + 1,
        2 * DAY_BLOCK_HOURS * HOUR,
    };
    std::string html;
    html.reserve(4096);
    appendPageStart(html, "Tarif", role, "Tarif");
    html += "<div class=\"card\"><p><strong>" + htmlEscape(tariffSummary()) + "</strong></p><table>";
    appendRow(html, "Freizeit", "bis und mit " + std::to_string(FREE_MINUTES) + " min: " + formatChf(0));
    appendRow(html, "Preis", formatChf(PRICE_PER_UNIT_RAPPEN) + " pro angefangene " + std::to_string(UNIT_MINUTES) +
                                 " min, ab Einfahrt gerechnet");
    appendRow(html, "Tagesmaximum",
              formatChf(DAILY_MAX_RAPPEN) + " pro angefangene " + std::to_string(DAY_BLOCK_HOURS) + " h ab Einfahrt");
    html += "</table></div><h2>Beispiele</h2><div class=\"scroll\"><table><tr><th>Parkdauer</th><th>Gebühr</th></tr>";
    for (int64_t d : examples)
    {
        uint32_t fee = 0;
        calculateFee(0, d, fee);
        html += "<tr><td>" + exampleDuration(d) + "</td><td>" + formatChf(fee) + "</td></tr>";
    }
    html += "</table></div>";
    appendPageEnd(html);
    return html;
}

namespace
{
// The Kasse (operator) and the Automat (customer) share the page; they differ in what they show.
struct PayPage
{
    const char* title;
    const char* path;      // search form
    const char* payAction; // pay form
    Role role;
    bool operatorView; // Kasse: any session (latest if none active), list of all sessions; Automat: only the
                       // typed plate's active session, tariff on top
};

std::string renderPayPage(const PayPage& page, const SessionStore& store, const std::string& query,
                          const Notice& notice, int64_t now, bool timeOk, const std::string& kasseError)
{
    std::string html;
    html.reserve(4096);
    appendPageStart(html, page.title, page.role, page.title);
    if (!page.operatorView)
    {
        html += "<p>Kennzeichen eingeben und Parkgebühr bezahlen. Tarif: " + htmlEscape(tariffSummary()) +
                " <a href=\"/tarif\">Tarif anzeigen</a></p>";
    }

    if (!kasseError.empty())
    {
        appendNotice(html, "err", kasseError + " – Parkvorgänge können nicht angezeigt werden");
    }
    else if (!timeOk)
    {
        appendNotice(html, "err", "Uhrzeit nicht synchronisiert – Gebühren und Zahlungen nicht möglich");
    }
    if (notice.cls[0] != '\0')
    {
        appendNotice(html, notice.cls, notice.text);
    }

    html += "<form method=\"get\" action=\"";
    html += page.path;
    html += "\"><label>Kennzeichen <input name=\"kz\" value=\"" + htmlEscape(query) +
            "\" placeholder=\"LU 123456\" autocomplete=\"off\" autofocus></label> "
            "<button type=\"submit\">Suchen</button></form>";
    if (!kasseError.empty())
    {
        appendPageEnd(html);
        return html;
    }

    std::string plate = normalizePlate(query);
    if (!plate.empty())
    {
        const ParkingSession* s = store.findActive(plate);
        if (s == nullptr && page.operatorView)
        {
            s = store.findLatest(plate);
        }
        if (!isValidPlate(plate))
        {
            appendNotice(html, "err", INVALID_PLATE_TEXT);
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
            if (page.operatorView)
            {
                html += "<p class=\"muted\"><a href=\"/tarif\">Tarif anzeigen</a></p>";
            }
            // Also offered when paid, so a second attempt answers «bereits bezahlt» (TF-20).
            if (timeOk && (v.payable || s->status == SessionStatus::Paid))
            {
                appendPayForm(html, v.plate, page.payAction);
            }
        }
    }

    if (page.operatorView)
    {
        appendSessionList(html, store, now);
    }
    if (timeOk)
    {
        html += "<p class=\"muted\">Stand: " + formatDateTime(now) + "</p>";
    }
    appendPageEnd(html);
    return html;
}
}

std::string renderKassePage(const SessionStore& store, const std::string& query, const Notice& notice, int64_t now,
                            bool timeOk, const std::string& kasseError)
{
    return renderPayPage({"Kasse", "/kasse", "/kasse/pay", Role::Admin, true}, store, query, notice, now, timeOk,
                         kasseError);
}

std::string renderAutomatPage(const SessionStore& store, const std::string& query, const Notice& notice,
                              int64_t now, bool timeOk, const std::string& kasseError, Role role)
{
    return renderPayPage({"Automat", "/automat", "/automat/pay", role, false}, store, query, notice, now, timeOk,
                         kasseError);
}

std::string renderKasseServerPage(const std::string& uiUrl)
{
    std::string url = htmlEscape(uiUrl);
    return "<!doctype html><html lang=\"de\"><head><meta charset=\"utf-8\">"
           "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>Kasse-Server – Parkhaus"
           "</title></head><body style=\"font-family:system-ui,sans-serif;max-width:860px;margin:0 auto;padding:16px\">"
           "<h1>Kasse-Server (Pico 1)</h1><p>Dieser Pico führt die Parkvorgänge und beantwortet die Anfragen der "
           "Schranken. Die Bedienung (Kamerasimulation, Kasse, Tarif, Betrieb) läuft auf Pico 2:</p><p><a href=\"" +
           url + "\">" + url + "</a></p></body></html>";
}
