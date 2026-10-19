#pragma once

#include <stddef.h>
#include <stdint.h>
#include <optional>
#include <string>
#include <vector>

constexpr size_t MAX_SESSIONS = 50;

enum class SessionStatus
{
    Open,
    Paid,
    Closed,
    ClosedFree,
};

enum class PaymentMethod
{
    None,
    Card,
    Twint,
    Cash,
};

struct Session
{
    std::string plate;
    int64_t entry = 0;
    int64_t exit = 0;
    uint32_t amountRappen = 0;
    uint32_t cashPaidRappen = 0;
    PaymentMethod method = PaymentMethod::None;
    std::string txRef;
    SessionStatus status = SessionStatus::Open;
};

// Open or paid: the plate is in the car park.
bool isActive(SessionStatus status);

// German UI labels: «offen», «bezahlt», «abgeschlossen», «abgeschlossen (Freizeit)».
const char* sessionStatusLabel(SessionStatus status);
// «Karte», «TWINT», «Bargeld»; empty for None.
const char* paymentMethodLabel(PaymentMethod method);

enum class EntryResult
{
    Ok,
    InvalidPlate,
    AlreadyOpen,
    EntryInFuture,
    Full,
};

struct ExitResult
{
    enum class Kind
    {
        Closed,
        ClosedFree,
        Unpaid,
        NoSession,
        InvalidPlate,
        TimeError,
    };
    Kind kind;
    uint32_t feeRappen = 0;
    int64_t exit = 0;
};

class SessionStore
{
public:
    // Opens a session stamped with now, or with backdatedEntry if given (must not be after now).
    // When the store is full, the oldest closed session is dropped; Full if none is closed.
    EntryResult openEntry(const std::string& rawPlate, int64_t now, std::optional<int64_t> backdatedEntry = {});

    // Paid -> Closed; open with fee 0 -> ClosedFree; open with fee > 0 -> Unpaid, nothing changes.
    ExitResult tryExit(const std::string& rawPlate, int64_t now);

    // Open or paid session for the plate, nullptr if none.
    Session* findActive(const std::string& rawPlate);
    const Session* findActive(const std::string& rawPlate) const;
    // Most recent session for the plate in any status, nullptr if none.
    const Session* findLatest(const std::string& rawPlate) const;

    const std::vector<Session>& all() const;

    // Next transaction reference ("TX-000001", ...), advances the counter.
    std::string takeTxRef();
    uint32_t nextTxNumber() const;

    // Replaces the whole state, e.g. after loading from flash or a test-data reset.
    void restore(std::vector<Session> loaded, uint32_t nextTx);

private:
    std::vector<Session> sessions;
    uint32_t nextTx = 1;
};

// Fee for an open session if it ended at now. False if now is before the entry time.
bool currentFee(const Session& session, int64_t now, uint32_t& feeRappen);
