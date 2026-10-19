#include "payment.h"

#include "plate.h"

PaymentResult pay(SessionStore& store, const std::string& rawPlate, PayMethod method, int64_t now,
                  uint32_t cashInsertedRappen)
{
    using Kind = PaymentResult::Kind;
    std::string plate = normalizePlate(rawPlate);
    if (!isValidPlate(plate))
    {
        return {Kind::InvalidPlate};
    }
    Session* s = store.findActive(plate);
    if (s == nullptr)
    {
        return {Kind::NoSession};
    }
    if (s->status == SessionStatus::Paid)
    {
        return {Kind::AlreadyPaid, s->amountRappen};
    }
    uint32_t fee = 0;
    if (!currentFee(*s, now, fee))
    {
        return {Kind::TimeError};
    }
    if (fee == 0)
    {
        return {Kind::NothingToPay};
    }

    uint32_t change = 0;
    PaymentMethod paidWith = PaymentMethod::None;
    switch (method)
    {
    case PayMethod::CardDeclined:
        return {Kind::Declined, fee};
    case PayMethod::CardOk:
        paidWith = PaymentMethod::Card;
        break;
    case PayMethod::Twint:
        paidWith = PaymentMethod::Twint;
        break;
    case PayMethod::Cash:
        if (cashInsertedRappen == 0)
        {
            return {Kind::InvalidAmount, fee, fee > s->cashPaidRappen ? fee - s->cashPaidRappen : 0};
        }
        s->cashPaidRappen += cashInsertedRappen;
        if (s->cashPaidRappen < fee)
        {
            return {Kind::PartialCash, fee, fee - s->cashPaidRappen};
        }
        change = s->cashPaidRappen - fee;
        paidWith = PaymentMethod::Cash;
        break;
    }

    s->status = SessionStatus::Paid;
    s->amountRappen = fee;
    s->method = paidWith;
    s->txRef = store.takeTxRef();
    return {Kind::Paid, fee, 0, change, paidWith, s->txRef};
}
