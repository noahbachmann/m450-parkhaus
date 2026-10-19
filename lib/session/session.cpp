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

bool currentFee(const Session& session, int64_t now, uint32_t& feeRappen)
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
    if (findActive(plate) != nullptr)
    {
        return EntryResult::AlreadyOpen;
    }
    int64_t entry = backdatedEntry.value_or(now);
    if (entry > now)
    {
        return EntryResult::EntryInFuture;
    }
    if (sessions.size() >= MAX_SESSIONS)
    {
        auto oldestClosed =
            std::find_if(sessions.begin(), sessions.end(), [](const Session& s) { return !isActive(s.status); });
        if (oldestClosed == sessions.end())
        {
            return EntryResult::Full;
        }
        sessions.erase(oldestClosed);
    }
    Session s;
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
    Session* s = findActive(plate);
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

Session* SessionStore::findActive(const std::string& rawPlate)
{
    return const_cast<Session*>(static_cast<const SessionStore*>(this)->findActive(rawPlate));
}

const Session* SessionStore::findActive(const std::string& rawPlate) const
{
    std::string plate = normalizePlate(rawPlate);
    auto it = std::find_if(sessions.begin(), sessions.end(),
                           [&](const Session& s) { return s.plate == plate && isActive(s.status); });
    return it == sessions.end() ? nullptr : &*it;
}

const Session* SessionStore::findLatest(const std::string& rawPlate) const
{
    std::string plate = normalizePlate(rawPlate);
    auto it = std::find_if(sessions.rbegin(), sessions.rend(), [&](const Session& s) { return s.plate == plate; });
    return it == sessions.rend() ? nullptr : &*it;
}

const std::vector<Session>& SessionStore::all() const
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

void SessionStore::restore(std::vector<Session> loaded, uint32_t nextTxNumber)
{
    sessions = std::move(loaded);
    nextTx = nextTxNumber;
}
