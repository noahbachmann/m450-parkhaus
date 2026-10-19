#include <unity.h>

#include "payment.h"
#include "session.h"

namespace
{
    constexpr int64_t NOW = 1760530000;
    constexpr int64_t MIN = 60;
    constexpr int64_t HOUR = 3600;

    using Kind = PaymentResult::Kind;
}

void setUp()
{
}

void tearDown()
{
}

void test_card_ok_pays_and_second_attempt_is_already_paid()
{
    SessionStore store;
    store.openEntry("LU 123456", NOW - 80 * MIN);
    PaymentResult r = pay(store, "LU 123456", PayMethod::CardOk, NOW);
    TEST_ASSERT_TRUE(Kind::Paid == r.kind);
    TEST_ASSERT_EQUAL_UINT32(400, r.amountRappen);
    TEST_ASSERT_TRUE(PaymentMethod::Card == r.method);
    TEST_ASSERT_EQUAL_STRING("TX-000001", r.txRef.c_str());

    const Session* s = store.findActive("LU123456");
    TEST_ASSERT_TRUE(SessionStatus::Paid == s->status);
    TEST_ASSERT_EQUAL_UINT32(400, s->amountRappen);
    TEST_ASSERT_TRUE(PaymentMethod::Card == s->method);
    TEST_ASSERT_EQUAL_STRING("TX-000001", s->txRef.c_str());

    PaymentResult again = pay(store, "LU123456", PayMethod::CardOk, NOW + MIN);
    TEST_ASSERT_TRUE(Kind::AlreadyPaid == again.kind);
    TEST_ASSERT_EQUAL_STRING("TX-000001", store.findActive("LU123456")->txRef.c_str());
    TEST_ASSERT_EQUAL_UINT32(2, store.nextTxNumber());
}

void test_card_declined_changes_nothing()
{
    SessionStore store;
    store.openEntry("LU 123456", NOW - 80 * MIN);
    PaymentResult r = pay(store, "LU123456", PayMethod::CardDeclined, NOW);
    TEST_ASSERT_TRUE(Kind::Declined == r.kind);
    TEST_ASSERT_EQUAL_UINT32(400, r.amountRappen);
    const Session* s = store.findActive("LU123456");
    TEST_ASSERT_TRUE(SessionStatus::Open == s->status);
    TEST_ASSERT_TRUE(s->txRef.empty());
    TEST_ASSERT_EQUAL_UINT32(1, store.nextTxNumber());
}

void test_twint()
{
    SessionStore store;
    store.openEntry("ZH 98765", NOW - (2 * HOUR + 30 * MIN));
    PaymentResult r = pay(store, "ZH98765", PayMethod::Twint, NOW);
    TEST_ASSERT_TRUE(Kind::Paid == r.kind);
    TEST_ASSERT_EQUAL_UINT32(600, r.amountRappen);
    TEST_ASSERT_TRUE(PaymentMethod::Twint == r.method);
}

void test_cash_with_change()
{
    SessionStore store;
    store.openEntry("LU 123456", NOW - (2 * HOUR + 30 * MIN));
    PaymentResult r = pay(store, "LU123456", PayMethod::Cash, NOW, 1000);
    TEST_ASSERT_TRUE(Kind::Paid == r.kind);
    TEST_ASSERT_EQUAL_UINT32(600, r.amountRappen);
    TEST_ASSERT_EQUAL_UINT32(400, r.changeRappen);
    TEST_ASSERT_TRUE(PaymentMethod::Cash == r.method);
    TEST_ASSERT_FALSE(r.txRef.empty());
}

void test_cash_partial_amounts_accumulate()
{
    SessionStore store;
    store.openEntry("AG 1", NOW - 30 * MIN);
    PaymentResult first = pay(store, "AG1", PayMethod::Cash, NOW, 100);
    TEST_ASSERT_TRUE(Kind::PartialCash == first.kind);
    TEST_ASSERT_EQUAL_UINT32(100, first.remainingRappen);
    TEST_ASSERT_TRUE(SessionStatus::Open == store.findActive("AG1")->status);

    PaymentResult second = pay(store, "AG1", PayMethod::Cash, NOW + MIN, 100);
    TEST_ASSERT_TRUE(Kind::Paid == second.kind);
    TEST_ASSERT_EQUAL_UINT32(200, second.amountRappen);
    TEST_ASSERT_EQUAL_UINT32(0, second.changeRappen);
}

void test_card_after_partial_cash_pays_the_rest()
{
    SessionStore store;
    store.openEntry("AG 1", NOW - 30 * MIN);
    pay(store, "AG1", PayMethod::Cash, NOW, 50);
    PaymentResult r = pay(store, "AG1", PayMethod::CardOk, NOW);
    TEST_ASSERT_TRUE(Kind::Paid == r.kind);
    TEST_ASSERT_EQUAL_UINT32(200, r.amountRappen);
    TEST_ASSERT_EQUAL_UINT32(0, r.changeRappen);
}

void test_cash_zero_is_invalid()
{
    SessionStore store;
    store.openEntry("AG 1", NOW - 30 * MIN);
    TEST_ASSERT_TRUE(Kind::InvalidAmount == pay(store, "AG1", PayMethod::Cash, NOW, 0).kind);
    TEST_ASSERT_TRUE(SessionStatus::Open == store.findActive("AG1")->status);
}

void test_no_session_and_invalid_plate()
{
    SessionStore store;
    TEST_ASSERT_TRUE(Kind::NoSession == pay(store, "SZ 555", PayMethod::CardOk, NOW).kind);
    TEST_ASSERT_TRUE(Kind::InvalidPlate == pay(store, "LU-12#45", PayMethod::CardOk, NOW).kind);
    TEST_ASSERT_EQUAL(0, store.all().size());
}

void test_nothing_to_pay_within_free_time()
{
    SessionStore store;
    store.openEntry("AG 1", NOW - 5 * MIN);
    TEST_ASSERT_TRUE(Kind::NothingToPay == pay(store, "AG1", PayMethod::CardOk, NOW).kind);
    TEST_ASSERT_TRUE(SessionStatus::Open == store.findActive("AG1")->status);
}

void test_tx_refs_are_unique()
{
    SessionStore store;
    store.openEntry("LU 123456", NOW - HOUR);
    store.openEntry("ZH 98765", NOW - HOUR);
    TEST_ASSERT_EQUAL_STRING("TX-000001", pay(store, "LU123456", PayMethod::CardOk, NOW).txRef.c_str());
    TEST_ASSERT_EQUAL_STRING("TX-000002", pay(store, "ZH98765", PayMethod::Twint, NOW).txRef.c_str());
}

void test_tf32_flow()
{
    SessionStore store;
    store.openEntry("OW 2026", NOW, NOW - (2 * HOUR + 30 * MIN));
    ExitResult first = store.tryExit("OW2026", NOW);
    TEST_ASSERT_TRUE(ExitResult::Kind::Unpaid == first.kind);
    TEST_ASSERT_EQUAL_UINT32(600, first.feeRappen);
    PaymentResult p = pay(store, "OW2026", PayMethod::Cash, NOW + MIN, 1000);
    TEST_ASSERT_EQUAL_UINT32(400, p.changeRappen);
    TEST_ASSERT_TRUE(ExitResult::Kind::Closed == store.tryExit("OW2026", NOW + 2 * MIN).kind);
    TEST_ASSERT_TRUE(SessionStatus::Closed == store.findLatest("OW2026")->status);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_card_ok_pays_and_second_attempt_is_already_paid);
    RUN_TEST(test_card_declined_changes_nothing);
    RUN_TEST(test_twint);
    RUN_TEST(test_cash_with_change);
    RUN_TEST(test_cash_partial_amounts_accumulate);
    RUN_TEST(test_card_after_partial_cash_pays_the_rest);
    RUN_TEST(test_cash_zero_is_invalid);
    RUN_TEST(test_no_session_and_invalid_plate);
    RUN_TEST(test_nothing_to_pay_within_free_time);
    RUN_TEST(test_tx_refs_are_unique);
    RUN_TEST(test_tf32_flow);
    return UNITY_END();
}
