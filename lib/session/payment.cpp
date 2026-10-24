#include "payment.h"

#include "plate.h"

const char* const PAYMENT_DECLINED_TEXT = "Zahlung abgelehnt – anderes Zahlungsmittel wählen";

bool parsePayMethod(const std::string& name, PayMethod& method)
{
    if (name == "card_ok")
    {
        method = PayMethod::CardOk;
    }
    else if (name == "card_declined")
    {
        method = PayMethod::CardDeclined;
    }
    else if (name == "twint")
    {
        method = PayMethod::Twint;
    }
    else if (name == "cash")
    {
        method = PayMethod::Cash;
    }
    else
    {
        return false;
    }
    return true;
}

PaymentResult pay(SessionStore& store, const std::string& rawPlate, PayMethod method, int64_t now,
                  uint32_t cashInsertedRappen)
{
    using Kind = PaymentResult::Kind;
    std::string plate = normalizePlate(rawPlate);
    if (!isValidPlate(plate))
    {
        return {Kind::InvalidPlate};
    }
    ParkingSession* s = store.findActive(plate);
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
    uint32_t cashPart = 0;
    PaymentMethod paidWith = PaymentMethod::None;
    switch (method)
    {
    case PayMethod::CardDeclined:
        return {Kind::Declined, fee};
    case PayMethod::CardOk:
        paidWith = PaymentMethod::Card;
        cashPart = s->cashPaidRappen;
        break;
    case PayMethod::Twint:
        paidWith = PaymentMethod::Twint;
        cashPart = s->cashPaidRappen;
        break;
    case PayMethod::Cash:
        if (cashInsertedRappen == 0 || cashInsertedRappen > MAX_CASH_RAPPEN)
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
    return {Kind::Paid, fee, 0, change, paidWith, s->txRef, cashPart};
}
