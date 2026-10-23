#pragma once

#include <stddef.h>
#include <stdint.h>
#include <optional>
#include <string>
#include <vector>

constexpr size_t MAX_SESSIONS = 50;
// A repeated entry within this time counts as the same entry (Pico 2 timed out, Pico 1 answered late).
constexpr int64_t REPEAT_ENTRY_SECONDS = 30;

enum class SessionStatus
{
    Open,
    Paid,
    Closed,
    ClosedFree,
    ClosedUnpaid, // operator opened the exit without payment
};

enum class PaymentMethod
{
    None,
    Card,
    Twint,
    Cash,
};

// Not named "Session": the core's WiFi.h pulls BearSSL::Session into the global namespace.
struct ParkingSession
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

// German UI labels: «offen», «bezahlt», «abgeschlossen», «abgeschlossen (Freizeit)», «ohne Zahlung ausgefahren».
const char* sessionStatusLabel(SessionStatus status);
// «Karte», «TWINT», «Bargeld»; empty for None.
const char* paymentMethodLabel(PaymentMethod method);

enum class EntryResult
{
    Ok,
    InvalidPlate,
    AlreadyOpen,
    Repeated, // same entry again (late answer + retry): the existing session counts, nothing changes
    EntryInFuture,
    Full,
};

struct ExitResult
{
    enum class Kind
    {
        Closed,
        ClosedFree,
        ClosedUnpaid,
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
    // An open, unpaid session for the plate makes a repeated request Repeated instead of AlreadyOpen if it
    // is the same entry: without backdating opened less than REPEAT_ENTRY_SECONDS ago, with backdating the
    // same entry time.
    EntryResult openEntry(const std::string& rawPlate, int64_t now, std::optional<int64_t> backdatedEntry = {});

    // Paid -> Closed; open with fee 0 -> ClosedFree; open with fee > 0 -> Unpaid, nothing changes.
    ExitResult tryExit(const std::string& rawPlate, int64_t now);
    // Operator remote opening at the exit: like tryExit, but a session that can't close normally (unpaid,
    // or clock before entry) closes as ClosedUnpaid; amountRappen keeps the unpaid fee.
    ExitResult forceExit(const std::string& rawPlate, int64_t now);

    // Open or paid session for the plate, nullptr if none.
    ParkingSession* findActive(const std::string& rawPlate);
    const ParkingSession* findActive(const std::string& rawPlate) const;
    // Most recent session for the plate in any status, nullptr if none.
    const ParkingSession* findLatest(const std::string& rawPlate) const;

    const std::vector<ParkingSession>& all() const;

    // Next transaction reference ("TX-000001", ...), advances the counter.
    std::string takeTxRef();
    uint32_t nextTxNumber() const;

    // Replaces the whole state, e.g. after loading from flash or a test-data reset.
    void restore(std::vector<ParkingSession> loaded, uint32_t nextTx);

private:
    std::vector<ParkingSession> sessions;
    uint32_t nextTx = 1;
};

// Fee for an open session if it ended at now. False if now is before the entry time.
bool currentFee(const ParkingSession& session, int64_t now, uint32_t& feeRappen);
