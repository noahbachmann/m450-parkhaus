#include "session.h"

#include <algorithm>
#include <cstdio>

#include "plate.h"
#include "tariff.h"

bool isActive(SessionStatus status)
{
    return status == SessionStatus::Open || status == SessionStatus::Paid;
}

const char* sessionStatusLabel(SessionStatus status)
{
    switch (status)
    {
    case SessionStatus::Open:
        return "offen";
    case SessionStatus::Paid:
        return "bezahlt";
    case SessionStatus::Closed:
        return "abgeschlossen";
    case SessionStatus::ClosedFree:
        return "abgeschlossen (Freizeit)";
    case SessionStatus::ClosedUnpaid:
        return "ohne Zahlung ausgefahren";
    }
    return "";
}

const char* paymentMethodLabel(PaymentMethod method)
{
    switch (method)
    {
    case PaymentMethod::Card:
        return "Karte";
    case PaymentMethod::Twint:
        return "TWINT";
    case PaymentMethod::Cash:
        return "Bargeld";
    case PaymentMethod::None:
        break;
    }
    return "";
}

namespace
{
    struct MethodName
    {
        PaymentMethod method;
        const char* name;
    };

    const MethodName METHOD_NAMES[] = {
        {PaymentMethod::None, "none"},
        {PaymentMethod::Card, "card"},
        {PaymentMethod::Twint, "twint"},
        {PaymentMethod::Cash, "cash"},
    };
}

const char* paymentMethodName(PaymentMethod method)
{
    for (const MethodName& m : METHOD_NAMES)
    {
        if (m.method == method)
        {
            return m.name;
        }
    }
    return "";
}

bool paymentMethodFromName(const std::string& name, PaymentMethod& method)
{
    for (const MethodName& m : METHOD_NAMES)
    {
        if (name == m.name)
        {
            method = m.method;
            return true;
        }
    }
    return false;
}

bool currentFee(const ParkingSession& session, int64_t now, uint32_t& feeRappen)
{
    return calculateFee(session.entry, now, feeRappen);
}

EntryResult SessionStore::openEntry(const std::string& rawPlate, int64_t now, std::optional<int64_t> backdatedEntry)
{
    std::string plate = normalizePlate(rawPlate);
    if (!isValidPlate(plate))
    {
        return EntryResult::InvalidPlate;
    }
    if (const ParkingSession* s = findActive(plate))
    {
        bool unpaid = s->status == SessionStatus::Open && s->cashPaidRappen == 0;
        bool sameEntry = backdatedEntry ? *backdatedEntry == s->entry
                                        : now >= s->entry && now - s->entry < REPEAT_ENTRY_SECONDS;
        return unpaid && sameEntry ? EntryResult::Repeated : EntryResult::AlreadyOpen;
    }
    int64_t entry = backdatedEntry.value_or(now);
    if (entry > now)
    {
        return EntryResult::EntryInFuture;
    }
    if (sessions.size() >= MAX_SESSIONS)
    {
        auto oldestClosed =
            std::find_if(sessions.begin(), sessions.end(), [](const ParkingSession& s) { return !isActive(s.status); });
        if (oldestClosed == sessions.end())
        {
            return EntryResult::Full;
        }
        sessions.erase(oldestClosed);
    }
    ParkingSession s;
    s.plate = plate;
    s.entry = entry;
    sessions.push_back(s);
    return EntryResult::Ok;
}

ExitResult SessionStore::tryExit(const std::string& rawPlate, int64_t now)
{
    std::string plate = normalizePlate(rawPlate);
    if (!isValidPlate(plate))
    {
        return {ExitResult::Kind::InvalidPlate};
    }
    ParkingSession* s = findActive(plate);
    if (s == nullptr)
    {
        return {ExitResult::Kind::NoSession};
    }
    if (s->status == SessionStatus::Paid)
    {
        s->status = SessionStatus::Closed;
        s->exit = now;
        return {ExitResult::Kind::Closed, s->amountRappen, now};
    }
    uint32_t fee = 0;
    if (!currentFee(*s, now, fee))
    {
        return {ExitResult::Kind::TimeError};
    }
    if (fee > 0)
    {
        return {ExitResult::Kind::Unpaid, fee};
    }
    s->status = SessionStatus::ClosedFree;
    s->exit = now;
    s->amountRappen = 0;
    return {ExitResult::Kind::ClosedFree, 0, now};
}

ExitResult SessionStore::forceExit(const std::string& rawPlate, int64_t now)
{
    ExitResult r = tryExit(rawPlate, now);
    if (r.kind != ExitResult::Kind::Unpaid && r.kind != ExitResult::Kind::TimeError)
    {
        return r;
    }
    ParkingSession* s = findActive(rawPlate);
    if (s == nullptr)
    {
        return {ExitResult::Kind::NoSession};
    }
    s->status = SessionStatus::ClosedUnpaid;
    s->exit = now;
    s->amountRappen = r.kind == ExitResult::Kind::Unpaid ? r.feeRappen : 0;
    return {ExitResult::Kind::ClosedUnpaid, s->amountRappen, now};
}

ParkingSession* SessionStore::findActive(const std::string& rawPlate)
{
    return const_cast<ParkingSession*>(static_cast<const SessionStore*>(this)->findActive(rawPlate));
}

const ParkingSession* SessionStore::findActive(const std::string& rawPlate) const
{
    std::string plate = normalizePlate(rawPlate);
    auto it = std::find_if(sessions.begin(), sessions.end(),
                           [&](const ParkingSession& s) { return s.plate == plate && isActive(s.status); });
    return it == sessions.end() ? nullptr : &*it;
}

const ParkingSession* SessionStore::findLatest(const std::string& rawPlate) const
{
    std::string plate = normalizePlate(rawPlate);
    auto it = std::find_if(sessions.rbegin(), sessions.rend(), [&](const ParkingSession& s) { return s.plate == plate; });
    return it == sessions.rend() ? nullptr : &*it;
}

const std::vector<ParkingSession>& SessionStore::all() const
{
    return sessions;
}

std::string SessionStore::takeTxRef()
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "TX-%06lu", static_cast<unsigned long>(nextTx));
    ++nextTx;
    return buf;
}

uint32_t SessionStore::nextTxNumber() const
{
    return nextTx;
}

void SessionStore::restore(std::vector<ParkingSession> loaded, uint32_t nextTxNumber)
{
    sessions = std::move(loaded);
    nextTx = nextTxNumber;
}
