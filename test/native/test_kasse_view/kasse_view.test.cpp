#include <unity.h>

#include "kasse_view.h"
#include "payment.h"
#include "session.h"

namespace
{
    constexpr int64_t NOW = 1760530000; // 15.10.2025 14:06:40 Zurich (CEST)
    constexpr int64_t MIN = 60;
    constexpr int64_t HOUR = 3600;
}

void setUp()
{
}

void tearDown()
{
}

void test_open_session_tf19()
{
    SessionStore store;
    store.openEntry("LU 123456", NOW - 80 * MIN);
    SessionView v = describeSession(*store.findActive("LU123456"), NOW);
    TEST_ASSERT_EQUAL_STRING("LU123456", v.plate.c_str());
    TEST_ASSERT_EQUAL_STRING("15.10.2025 12:46:40", v.entry.c_str());
    TEST_ASSERT_EQUAL_STRING("", v.exit.c_str());
    TEST_ASSERT_EQUAL_STRING("1 h 20 min", v.duration.c_str());
    TEST_ASSERT_EQUAL_STRING("CHF 4.00", v.fee.c_str());
    TEST_ASSERT_EQUAL_STRING("offen", v.status.c_str());
    TEST_ASSERT_EQUAL_STRING("unbezahlt", v.paymentStatus.c_str());
    TEST_ASSERT_EQUAL_STRING("2 angefangene Stunden à CHF 2.00 ab Einfahrt (15 Freiminuten überschritten)",
                             v.tariffNote.c_str());
    TEST_ASSERT_TRUE(v.payable);
}

void test_free_time_is_not_payable()
{
    SessionStore store;
    store.openEntry("AG1", NOW - 5 * MIN);
    SessionView v = describeSession(*store.findActive("AG1"), NOW);
    TEST_ASSERT_EQUAL_STRING("CHF 0.00", v.fee.c_str());
    TEST_ASSERT_EQUAL_STRING("innerhalb der 15 Freiminuten", v.tariffNote.c_str());
    TEST_ASSERT_FALSE(v.payable);
}

void test_paid_and_closed_session()
{
    SessionStore store;
    store.openEntry("LU123456", NOW - 80 * MIN);
    pay(store, "LU123456", PayMethod::CardOk, NOW);
    SessionView paid = describeSession(*store.findActive("LU123456"), NOW + MIN);
    TEST_ASSERT_EQUAL_STRING("bezahlt", paid.status.c_str());
    TEST_ASSERT_EQUAL_STRING("bezahlt", paid.paymentStatus.c_str());
    TEST_ASSERT_EQUAL_STRING("CHF 4.00", paid.fee.c_str());
    TEST_ASSERT_EQUAL_STRING("Karte, TX-000001", paid.payment.c_str());
    TEST_ASSERT_FALSE(paid.payable);

    store.tryExit("LU123456", NOW + 2 * MIN);
    SessionView closed = describeSession(*store.findLatest("LU123456"), NOW + HOUR);
    TEST_ASSERT_EQUAL_STRING("abgeschlossen", closed.status.c_str());
    TEST_ASSERT_EQUAL_STRING("15.10.2025 14:08:40", closed.exit.c_str());
    TEST_ASSERT_EQUAL_STRING("1 h 22 min", closed.duration.c_str());
    TEST_ASSERT_EQUAL_STRING("", closed.paymentStatus.c_str());
    TEST_ASSERT_EQUAL_STRING("", closed.tariffNote.c_str());
}

void test_partial_cash_is_shown()
{
    SessionStore store;
    store.openEntry("AG1", NOW - 30 * MIN);
    pay(store, "AG1", PayMethod::Cash, NOW, 100);
    SessionView v = describeSession(*store.findActive("AG1"), NOW);
    TEST_ASSERT_EQUAL_STRING("CHF 1.00", v.cashInserted.c_str());
    TEST_ASSERT_EQUAL_STRING("unbezahlt", v.paymentStatus.c_str());
}

void test_clock_before_entry_shows_no_fee()
{
    SessionStore store;
    store.openEntry("LU123456", NOW);
    SessionView v = describeSession(*store.findActive("LU123456"), NOW - HOUR);
    TEST_ASSERT_EQUAL_STRING("", v.fee.c_str());
    TEST_ASSERT_FALSE(v.payable);
}

void test_tariff_note_daily_max()
{
    TEST_ASSERT_EQUAL_STRING("1 angefangene Stunde à CHF 2.00 ab Einfahrt (15 Freiminuten überschritten)",
                             tariffNote(16 * MIN, 200).c_str());
    TEST_ASSERT_EQUAL_STRING("11 angefangene Stunden à CHF 2.00 ab Einfahrt (15 Freiminuten überschritten), "
                             "Tagesmaximum CHF 20.00 pro angefangene 24 h",
                             tariffNote(10 * HOUR + 1, 2000).c_str());
}

void test_payment_messages_tf20_tf21_tf22()
{
    SessionStore store;
    store.openEntry("LU123456", NOW - 80 * MIN);
    PaymentResult card = pay(store, "LU123456", PayMethod::CardOk, NOW);
    TEST_ASSERT_EQUAL_STRING("Bezahlt – CHF 4.00, Karte – Transaktion TX-000001",
                             paymentMessage(card, "LU123456").c_str());
    TEST_ASSERT_EQUAL_STRING("bereits bezahlt",
                             paymentMessage(pay(store, "LU123456", PayMethod::CardOk, NOW), "LU123456").c_str());

    store.openEntry("AG1", NOW - 30 * MIN);
    PaymentResult partial = pay(store, "AG1", PayMethod::Cash, NOW, 100);
    TEST_ASSERT_EQUAL_STRING("Restbetrag CHF 1.00 – unbezahlt", paymentMessage(partial, "AG1").c_str());
    TEST_ASSERT_TRUE(paymentSucceeded(partial));
    PaymentResult rest = pay(store, "AG1", PayMethod::Cash, NOW, 100);
    TEST_ASSERT_EQUAL_STRING("Bezahlt – CHF 2.00, Bargeld – Transaktion TX-000002 – Rückgeld CHF 0.00",
                             paymentMessage(rest, "AG1").c_str());

    PaymentResult none = pay(store, "SZ 555", PayMethod::CardOk, NOW);
    TEST_ASSERT_EQUAL_STRING("Kein offener Parkvorgang für SZ555", paymentMessage(none, "SZ555").c_str());
    TEST_ASSERT_FALSE(paymentSucceeded(none));

    store.openEntry("ZH98765", NOW - 80 * MIN);
    PaymentResult declined = pay(store, "ZH98765", PayMethod::CardDeclined, NOW);
    TEST_ASSERT_EQUAL_STRING("Zahlung abgelehnt – anderes Zahlungsmittel wählen",
                             paymentMessage(declined, "ZH98765").c_str());
    TEST_ASSERT_FALSE(paymentSucceeded(declined));
}

void test_parse_chf()
{
    uint32_t r = 0;
    TEST_ASSERT_TRUE(parseChf("10", r));
    TEST_ASSERT_EQUAL_UINT32(1000, r);
    TEST_ASSERT_TRUE(parseChf(" 10.5 ", r));
    TEST_ASSERT_EQUAL_UINT32(1050, r);
    TEST_ASSERT_TRUE(parseChf("1,05", r));
    TEST_ASSERT_EQUAL_UINT32(105, r);
    TEST_ASSERT_TRUE(parseChf("CHF 10.00", r));
    TEST_ASSERT_EQUAL_UINT32(1000, r);
    TEST_ASSERT_TRUE(parseChf("1000", r));
    TEST_ASSERT_EQUAL_UINT32(100000, r);

    r = 7;
    TEST_ASSERT_FALSE(parseChf("", r));
    TEST_ASSERT_FALSE(parseChf("0", r));
    TEST_ASSERT_FALSE(parseChf("0.00", r));
    TEST_ASSERT_FALSE(parseChf("-5", r));
    TEST_ASSERT_FALSE(parseChf("10.", r));
    TEST_ASSERT_FALSE(parseChf("10.123", r));
    TEST_ASSERT_FALSE(parseChf("1e3", r));
    TEST_ASSERT_FALSE(parseChf("abc", r));
    TEST_ASSERT_FALSE(parseChf("1000.01", r));
    TEST_ASSERT_FALSE(parseChf("99999999999999999999", r));
    TEST_ASSERT_EQUAL_UINT32(7, r);
}

void test_html_escape()
{
    TEST_ASSERT_EQUAL_STRING("&lt;b&gt; &amp; &quot;x&quot; &#39;", htmlEscape("<b> & \"x\" '").c_str());
    TEST_ASSERT_EQUAL_STRING("LU123456", htmlEscape("LU123456").c_str());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_open_session_tf19);
    RUN_TEST(test_free_time_is_not_payable);
    RUN_TEST(test_paid_and_closed_session);
    RUN_TEST(test_partial_cash_is_shown);
    RUN_TEST(test_clock_before_entry_shows_no_fee);
    RUN_TEST(test_tariff_note_daily_max);
    RUN_TEST(test_payment_messages_tf20_tf21_tf22);
    RUN_TEST(test_parse_chf);
    RUN_TEST(test_html_escape);
    return UNITY_END();
}
