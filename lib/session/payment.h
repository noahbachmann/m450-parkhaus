#pragma once

#include <stdint.h>
#include <string>

#include "session.h"

constexpr uint32_t MAX_CASH_RAPPEN = 100000; // CHF 1000.00 per insertion

// Shown at the Kasse, the Automat and the exit terminal (TF-28).
extern const char* const PAYMENT_DECLINED_TEXT;

// Simulated payment methods ZM-01..ZM-04 from the test concept.
enum class PayMethod
{
    CardOk,
    CardDeclined,
    Twint,
    Cash,
};

struct PaymentResult
{
    enum class Kind
    {
        Paid,
        Declined,
        PartialCash,
        AlreadyPaid,
        NoSession,
        NothingToPay,
        InvalidPlate,
        InvalidAmount,
        TimeError,
    };
    Kind kind;
    uint32_t amountRappen = 0;
    uint32_t remainingRappen = 0;
    uint32_t changeRappen = 0;
    PaymentMethod method = PaymentMethod::None;
    std::string txRef = {};
    uint32_t cashRappen = 0; // card/TWINT: cash inserted earlier that counts towards amountRappen
};

// Form and API values "card_ok", "card_declined", "twint", "cash". False for anything else.
bool parsePayMethod(const std::string& name, PayMethod& method);

// Pays the open session of the plate with the fee at now.
// Cash: cashInsertedRappen (1..MAX_CASH_RAPPEN, else InvalidAmount) accumulates until the fee is covered,
// then change = inserted total - fee.
// Card/TWINT charge whatever is still outstanding after earlier partial cash; amountRappen is always the
// whole fee booked on the session.
PaymentResult pay(SessionStore& store, const std::string& rawPlate, PayMethod method, int64_t now,
                  uint32_t cashInsertedRappen = 0);
