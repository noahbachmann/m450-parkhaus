#include <unity.h>

#include <string>

#include "payment.h"
#include "session.h"

namespace
{
    constexpr int64_t NOW = 1760530000;
    constexpr int64_t MIN = 60;
    constexpr int64_t HOUR = 3600;
}

void setUp()
{
}

void tearDown()
{
}

void test_entry_opens_session_stamped_now()
{
    SessionStore store;
    TEST_ASSERT_TRUE(EntryResult::Ok == store.openEntry("ZH 98765", NOW));
    const ParkingSession* s = store.findActive("ZH98765");
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_STRING("ZH98765", s->plate.c_str());
    TEST_ASSERT_EQUAL_INT64(NOW, s->entry);
    TEST_ASSERT_EQUAL_INT64(0, s->exit);
    TEST_ASSERT_TRUE(SessionStatus::Open == s->status);
}

void test_backdated_entry()
{
    SessionStore store;
    TEST_ASSERT_TRUE(EntryResult::Ok == store.openEntry("OW 2026", NOW, NOW - (2 * HOUR + 30 * MIN)));
    TEST_ASSERT_EQUAL_INT64(NOW - (2 * HOUR + 30 * MIN), store.findActive("OW2026")->entry);
    TEST_ASSERT_TRUE(EntryResult::Ok == store.openEntry("UR 1", NOW, NOW));
}

void test_entry_in_future_is_rejected()
{
    SessionStore store;
    TEST_ASSERT_TRUE(EntryResult::EntryInFuture == store.openEntry("LU 123456", NOW, NOW + 1));
    TEST_ASSERT_TRUE(EntryResult::EntryInFuture == store.openEntry("LU 123456", NOW, NOW + HOUR));
    TEST_ASSERT_EQUAL(0, store.all().size());
}

void test_invalid_plate_is_rejected()
{
    SessionStore store;
    TEST_ASSERT_TRUE(EntryResult::InvalidPlate == store.openEntry("", NOW));
    TEST_ASSERT_TRUE(EntryResult::InvalidPlate == store.openEntry("LU-12#45", NOW));
    TEST_ASSERT_TRUE(ExitResult::Kind::InvalidPlate == store.tryExit("L 123", NOW).kind);
    TEST_ASSERT_EQUAL(0, store.all().size());
}

void test_second_entry_while_open_is_rejected()
{
    SessionStore store;
    store.openEntry("ZH 98765", NOW);
    TEST_ASSERT_TRUE(EntryResult::AlreadyOpen == store.openEntry("zh98765", NOW + MIN));
    TEST_ASSERT_EQUAL(1, store.all().size());
    TEST_ASSERT_EQUAL_INT64(NOW, store.findActive("ZH98765")->entry);
}

void test_repeated_entry_after_late_answer()
{
    SessionStore store;
    store.openEntry("ZH 98765", NOW);
    TEST_ASSERT_TRUE(EntryResult::Repeated == store.openEntry("ZH98765", NOW + 2));
    TEST_ASSERT_TRUE(EntryResult::Repeated == store.openEntry("ZH98765", NOW + REPEAT_ENTRY_SECONDS - 1));
    TEST_ASSERT_TRUE(EntryResult::AlreadyOpen == store.openEntry("ZH98765", NOW + REPEAT_ENTRY_SECONDS));
    TEST_ASSERT_EQUAL(1, store.all().size());
    TEST_ASSERT_EQUAL_INT64(NOW, store.findActive("ZH98765")->entry);

    store.openEntry("LU123456", NOW, NOW - HOUR); // backdated: same entry time = same entry
    TEST_ASSERT_TRUE(EntryResult::Repeated == store.openEntry("LU123456", NOW + 2, NOW - HOUR));
    TEST_ASSERT_TRUE(EntryResult::AlreadyOpen == store.openEntry("LU123456", NOW + 2, NOW - 2 * HOUR));
    TEST_ASSERT_TRUE(EntryResult::AlreadyOpen == store.openEntry("LU123456", NOW + 2));
}

void test_repeated_entry_not_after_payment()
{
    SessionStore store;
    store.openEntry("AG1", NOW - 20 * MIN);
    pay(store, "AG1", PayMethod::Cash, NOW, 100); // partial cash
    TEST_ASSERT_TRUE(EntryResult::AlreadyOpen == store.openEntry("AG1", NOW, NOW - 20 * MIN));
    store.openEntry("UR1", NOW - 2);
    pay(store, "UR1", PayMethod::CardOk, NOW); // nothing to pay in free time: stays open, unpaid
    TEST_ASSERT_TRUE(EntryResult::Repeated == store.openEntry("UR1", NOW));
}

void test_second_entry_while_paid_is_rejected()
{
    SessionStore store;
    store.openEntry("LU 123456", NOW - HOUR);
    pay(store, "LU123456", PayMethod::CardOk, NOW);
    TEST_ASSERT_TRUE(EntryResult::AlreadyOpen == store.openEntry("LU 123456", NOW));
}

void test_normalized_entry_and_exit_match()
{
    SessionStore store;
    store.openEntry("lu 123 456", NOW - HOUR);
    pay(store, "LU123456", PayMethod::CardOk, NOW);
    ExitResult r = store.tryExit(" LU123456 ", NOW + MIN);
    TEST_ASSERT_TRUE(ExitResult::Kind::Closed == r.kind);
    TEST_ASSERT_EQUAL(1, store.all().size());
}

void test_paid_exit_closes_with_exit_time()
{
    SessionStore store;
    store.openEntry("LU 123456", NOW - 80 * MIN);
    pay(store, "LU123456", PayMethod::CardOk, NOW);
    ExitResult r = store.tryExit("LU123456", NOW + 3 * HOUR);
    TEST_ASSERT_TRUE(ExitResult::Kind::Closed == r.kind);
    TEST_ASSERT_EQUAL_UINT32(400, r.feeRappen);
    TEST_ASSERT_EQUAL_INT64(NOW + 3 * HOUR, r.exit);
    TEST_ASSERT_NULL(store.findActive("LU123456"));
    const ParkingSession* s = store.findLatest("LU123456");
    TEST_ASSERT_TRUE(SessionStatus::Closed == s->status);
    TEST_ASSERT_EQUAL_INT64(NOW + 3 * HOUR, s->exit);
}

void test_exit_within_free_time_closes_free()
{
    SessionStore store;
    store.openEntry("AG 1", NOW - 5 * MIN);
    ExitResult r = store.tryExit("AG1", NOW);
    TEST_ASSERT_TRUE(ExitResult::Kind::ClosedFree == r.kind);
    TEST_ASSERT_EQUAL_UINT32(0, r.feeRappen);
    const ParkingSession* s = store.findLatest("AG1");
    TEST_ASSERT_TRUE(SessionStatus::ClosedFree == s->status);
    TEST_ASSERT_EQUAL_INT64(NOW, s->exit);
    TEST_ASSERT_EQUAL_UINT32(0, s->amountRappen);
}

void test_unpaid_exit_changes_nothing_and_fee_keeps_growing()
{
    SessionStore store;
    store.openEntry("ZH 98765", NOW - 58 * MIN);
    ExitResult r = store.tryExit("ZH98765", NOW);
    TEST_ASSERT_TRUE(ExitResult::Kind::Unpaid == r.kind);
    TEST_ASSERT_EQUAL_UINT32(200, r.feeRappen);
    const ParkingSession* s = store.findActive("ZH98765");
    TEST_ASSERT_TRUE(SessionStatus::Open == s->status);
    TEST_ASSERT_EQUAL_INT64(0, s->exit);
    TEST_ASSERT_EQUAL_UINT32(400, store.tryExit("ZH98765", NOW + 3 * MIN).feeRappen);
}

void test_exit_without_session()
{
    SessionStore store;
    TEST_ASSERT_TRUE(ExitResult::Kind::NoSession == store.tryExit("SZ 555", NOW).kind);
    store.openEntry("AG 1", NOW);
    store.tryExit("AG 1", NOW);
    TEST_ASSERT_TRUE(ExitResult::Kind::NoSession == store.tryExit("AG 1", NOW).kind);
}

void test_exit_with_clock_before_entry_stays_closed()
{
    SessionStore store;
    store.openEntry("ZH 98765", NOW);
    TEST_ASSERT_TRUE(ExitResult::Kind::TimeError == store.tryExit("ZH98765", NOW - 1).kind);
    TEST_ASSERT_TRUE(SessionStatus::Open == store.findActive("ZH98765")->status);
}

void test_new_session_after_closed_one()
{
    SessionStore store;
    store.openEntry("AG 1", NOW - 5 * MIN);
    store.tryExit("AG1", NOW);
    TEST_ASSERT_TRUE(EntryResult::Ok == store.openEntry("AG 1", NOW + MIN));
    TEST_ASSERT_EQUAL(2, store.all().size());
    TEST_ASSERT_EQUAL_INT64(NOW + MIN, store.findLatest("AG1")->entry);
    TEST_ASSERT_TRUE(SessionStatus::Open == store.findLatest("AG1")->status);
}

void test_full_store_drops_oldest_closed()
{
    SessionStore store;
    store.openEntry("AG 1", NOW - 5 * MIN);
    store.tryExit("AG 1", NOW);
    for (size_t i = 1; i < MAX_SESSIONS; ++i)
    {
        TEST_ASSERT_TRUE(EntryResult::Ok == store.openEntry("ZH" + std::to_string(i), NOW));
    }
    TEST_ASSERT_EQUAL(MAX_SESSIONS, store.all().size());
    TEST_ASSERT_TRUE(EntryResult::Ok == store.openEntry("UR 1", NOW));
    TEST_ASSERT_EQUAL(MAX_SESSIONS, store.all().size());
    TEST_ASSERT_NULL(store.findLatest("AG1"));
    TEST_ASSERT_TRUE(EntryResult::Full == store.openEntry("SZ 555", NOW));
    TEST_ASSERT_NULL(store.findLatest("SZ555"));
}

void test_status_labels()
{
    TEST_ASSERT_EQUAL_STRING("offen", sessionStatusLabel(SessionStatus::Open));
    TEST_ASSERT_EQUAL_STRING("bezahlt", sessionStatusLabel(SessionStatus::Paid));
    TEST_ASSERT_EQUAL_STRING("abgeschlossen", sessionStatusLabel(SessionStatus::Closed));
    TEST_ASSERT_EQUAL_STRING("abgeschlossen (Freizeit)", sessionStatusLabel(SessionStatus::ClosedFree));
    TEST_ASSERT_EQUAL_STRING("Karte", paymentMethodLabel(PaymentMethod::Card));
    TEST_ASSERT_EQUAL_STRING("TWINT", paymentMethodLabel(PaymentMethod::Twint));
    TEST_ASSERT_EQUAL_STRING("Bargeld", paymentMethodLabel(PaymentMethod::Cash));
}

void test_force_exit()
{
    SessionStore store;
    store.openEntry("ZH98765", NOW - 150 * MIN);
    ExitResult unpaid = store.forceExit("ZH98765", NOW);
    TEST_ASSERT_TRUE(ExitResult::Kind::ClosedUnpaid == unpaid.kind);
    TEST_ASSERT_EQUAL_UINT32(600, unpaid.feeRappen);
    const ParkingSession* s = store.findLatest("ZH98765");
    TEST_ASSERT_TRUE(SessionStatus::ClosedUnpaid == s->status);
    TEST_ASSERT_EQUAL_INT64(NOW, s->exit);
    TEST_ASSERT_EQUAL_UINT32(600, s->amountRappen);
    TEST_ASSERT_NULL(store.findActive("ZH98765"));

    store.openEntry("AG1", NOW - 5 * MIN);
    TEST_ASSERT_TRUE(ExitResult::Kind::ClosedFree == store.forceExit("AG1", NOW).kind);
    store.openEntry("UR1", NOW);
    TEST_ASSERT_TRUE(ExitResult::Kind::ClosedUnpaid == store.forceExit("UR1", NOW - HOUR).kind);
    TEST_ASSERT_TRUE(ExitResult::Kind::NoSession == store.forceExit("SZ555", NOW).kind);
    TEST_ASSERT_EQUAL_STRING("ohne Zahlung ausgefahren", sessionStatusLabel(SessionStatus::ClosedUnpaid));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_entry_opens_session_stamped_now);
    RUN_TEST(test_backdated_entry);
    RUN_TEST(test_entry_in_future_is_rejected);
    RUN_TEST(test_invalid_plate_is_rejected);
    RUN_TEST(test_second_entry_while_open_is_rejected);
    RUN_TEST(test_repeated_entry_after_late_answer);
    RUN_TEST(test_repeated_entry_not_after_payment);
    RUN_TEST(test_second_entry_while_paid_is_rejected);
    RUN_TEST(test_normalized_entry_and_exit_match);
    RUN_TEST(test_paid_exit_closes_with_exit_time);
    RUN_TEST(test_exit_within_free_time_closes_free);
    RUN_TEST(test_unpaid_exit_changes_nothing_and_fee_keeps_growing);
    RUN_TEST(test_exit_without_session);
    RUN_TEST(test_exit_with_clock_before_entry_stays_closed);
    RUN_TEST(test_new_session_after_closed_one);
    RUN_TEST(test_full_store_drops_oldest_closed);
    RUN_TEST(test_status_labels);
    RUN_TEST(test_force_exit);
    return UNITY_END();
}
