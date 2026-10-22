#include "kasse_view.h"

#include <cctype>

#include "format.h"
#include "tariff.h"

namespace
{
    constexpr uint32_t MAX_CASH_RAPPEN = 100000;

    std::string trim(const std::string& s)
    {
        size_t begin = s.find_first_not_of(" \t");
        if (begin == std::string::npos)
        {
            return "";
        }
        size_t end = s.find_last_not_of(" \t");
        return s.substr(begin, end - begin + 1);
    }
}

SessionView describeSession(const ParkingSession& session, int64_t now)
{
    SessionView v;
    v.plate = session.plate;
    v.entry = formatDateTime(session.entry);
    v.status = sessionStatusLabel(session.status);
    bool active = isActive(session.status);
    int64_t end = active ? now : session.exit;
    v.duration = formatDuration(end - session.entry);
    if (!active)
    {
        v.exit = formatDateTime(session.exit);
    }

    if (session.status == SessionStatus::Open)
    {
        v.paymentStatus = "unbezahlt";
        uint32_t fee = 0;
        if (currentFee(session, now, fee))
        {
            v.fee = formatChf(fee);
            v.tariffNote = tariffNote(now - session.entry, fee);
            v.payable = fee > 0;
        }
        if (session.cashPaidRappen > 0)
        {
            v.cashInserted = formatChf(session.cashPaidRappen);
        }
    }
    else
    {
        v.fee = formatChf(session.amountRappen);
        if (session.status == SessionStatus::Paid)
        {
            v.paymentStatus = "bezahlt";
            v.tariffNote = tariffNote(now - session.entry, session.amountRappen);
        }
    }
    if (session.method != PaymentMethod::None)
    {
        v.payment = std::string(paymentMethodLabel(session.method)) + ", " + session.txRef;
    }
    return v;
}

std::string tariffNote(int64_t durationSeconds, uint32_t feeRappen)
{
    if (durationSeconds <= FREE_MINUTES * 60)
    {
        return "innerhalb der " + std::to_string(FREE_MINUTES) + " Freiminuten";
    }
    int64_t unitSeconds = UNIT_MINUTES * 60;
    int64_t units = (durationSeconds + unitSeconds - 1) / unitSeconds;
    std::string note = std::to_string(units) + (units == 1 ? " angefangene Stunde à " : " angefangene Stunden à ") +
                       formatChf(PRICE_PER_UNIT_RAPPEN) + " ab Einfahrt (" + std::to_string(FREE_MINUTES) +
                       " Freiminuten überschritten)";
    if (feeRappen < static_cast<uint64_t>(units) * PRICE_PER_UNIT_RAPPEN)
    {
        note += ", Tagesmaximum " + formatChf(DAILY_MAX_RAPPEN) + " pro angefangene " +
                std::to_string(DAY_BLOCK_HOURS) + " h";
    }
    return note;
}

std::string paymentMessage(const PaymentResult& result, const std::string& plate)
{
    using Kind = PaymentResult::Kind;
    switch (result.kind)
    {
    case Kind::Paid:
    {
        std::string msg = "Bezahlt – " + formatChf(result.amountRappen) + ", " + paymentMethodLabel(result.method) +
                          " – Transaktion " + result.txRef;
        if (result.method == PaymentMethod::Cash)
        {
            msg += " – Rückgeld " + formatChf(result.changeRappen);
        }
        return msg;
    }
    case Kind::Declined:
        return "Zahlung abgelehnt – anderes Zahlungsmittel wählen";
    case Kind::PartialCash:
        return "Restbetrag " + formatChf(result.remainingRappen) + " – unbezahlt";
    case Kind::AlreadyPaid:
        return "bereits bezahlt";
    case Kind::NoSession:
        return noSessionMessage(plate);
    case Kind::NothingToPay:
        return "Keine Gebühr (Freizeit) – nichts zu bezahlen";
    case Kind::InvalidPlate:
        return "Ungültiges Kennzeichen";
    case Kind::InvalidAmount:
        return "Bitte eingeworfenen Betrag in CHF eingeben";
    case Kind::TimeError:
        return "Uhrzeit nicht synchronisiert – bitte später erneut versuchen";
    }
    return "";
}

bool paymentSucceeded(const PaymentResult& result)
{
    return result.kind == PaymentResult::Kind::Paid || result.kind == PaymentResult::Kind::PartialCash;
}

std::string noSessionMessage(const std::string& plate)
{
    return "Kein offener Parkvorgang für " + plate;
}

bool parseChf(const std::string& text, uint32_t& rappen)
{
    std::string s = trim(text);
    if (s.size() >= 3 && (s.compare(0, 3, "CHF") == 0 || s.compare(0, 3, "chf") == 0))
    {
        s = trim(s.substr(3));
    }
    uint64_t francs = 0;
    size_t i = 0;
    for (; i < s.size() && std::isdigit(static_cast<unsigned char>(s[i])); ++i)
    {
        francs = francs * 10 + static_cast<uint64_t>(s[i] - '0');
        if (francs > MAX_CASH_RAPPEN)
        {
            return false;
        }
    }
    if (i == 0)
    {
        return false;
    }
    uint64_t cents = 0;
    if (i < s.size())
    {
        if (s[i] != '.' && s[i] != ',')
        {
            return false;
        }
        size_t digits = s.size() - i - 1;
        if (digits < 1 || digits > 2)
        {
            return false;
        }
        for (size_t k = i + 1; k < s.size(); ++k)
        {
            if (!std::isdigit(static_cast<unsigned char>(s[k])))
            {
                return false;
            }
            cents = cents * 10 + static_cast<uint64_t>(s[k] - '0');
        }
        if (digits == 1)
        {
            cents *= 10;
        }
    }
    uint64_t total = francs * 100 + cents;
    if (total == 0 || total > MAX_CASH_RAPPEN)
    {
        return false;
    }
    rappen = static_cast<uint32_t>(total);
    return true;
}

std::string htmlEscape(const std::string& text)
{
    std::string out;
    out.reserve(text.size());
    for (char c : text)
    {
        switch (c)
        {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        case '\'':
            out += "&#39;";
            break;
        default:
            out += c;
        }
    }
    return out;
}
