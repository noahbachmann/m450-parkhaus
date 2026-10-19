#pragma once

#include <stdint.h>
#include <string>

#include "session.h"

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
};

// Pays the open session of the plate with the fee at now.
// Cash: cashInsertedRappen accumulates until the fee is covered, then change = inserted total - fee.
// Card/TWINT charge whatever is still outstanding after earlier partial cash.
PaymentResult pay(SessionStore& store, const std::string& rawPlate, PayMethod method, int64_t now,
                  uint32_t cashInsertedRappen = 0);
