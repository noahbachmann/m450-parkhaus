#include <unity.h>

#include "kasse_pages.h"
#include "kasse_view.h"
#include "payment.h"
#include "session.h"

namespace
{
    constexpr int64_t NOW = 1760530000; // 15.10.2025 14:06:40 Zurich (CEST)
    constexpr int64_t MIN = 60;
    constexpr int64_t HOUR = 3600;

    bool contains(const std::string& html, const char* text)
    {
        return html.find(text) != std::string::npos;
    }
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
                             tariffNote(16 * MIN).c_str());
    TEST_ASSERT_EQUAL_STRING("11 angefangene Stunden à CHF 2.00 ab Einfahrt (15 Freiminuten überschritten), "
                             "Tagesmaximum CHF 20.00 pro angefangene 24 h",
                             tariffNote(10 * HOUR + 1).c_str());
    TEST_ASSERT_EQUAL_STRING("10 angefangene Stunden à CHF 2.00 ab Einfahrt (15 Freiminuten überschritten)",
                             tariffNote(10 * HOUR).c_str());
}

void test_tariff_note_beyond_one_day()
{
    TEST_ASSERT_EQUAL_STRING("1 × 24 h à CHF 20.00, danach 1 angefangene Stunde à CHF 2.00 "
                             "(15 Freiminuten überschritten)",
                             tariffNote(24 * HOUR + 1).c_str());
    TEST_ASSERT_EQUAL_STRING("2 × 24 h à CHF 20.00 (15 Freiminuten überschritten)", tariffNote(48 * HOUR).c_str());
}

void test_paid_session_has_no_running_tariff_note()
{
    SessionStore store;
    store.openEntry("LU123456", NOW - 90 * MIN);
    pay(store, "LU123456", PayMethod::CardOk, NOW);
    SessionView v = describeSession(*store.findActive("LU123456"), NOW + 90 * MIN);
    TEST_ASSERT_EQUAL_STRING("CHF 4.00", v.fee.c_str());
    TEST_ASSERT_EQUAL_STRING("", v.tariffNote.c_str());
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

    store.openEntry("UR1", NOW - 30 * MIN);
    pay(store, "UR1", PayMethod::Cash, NOW, 50);
    PaymentResult mixed = pay(store, "UR1", PayMethod::Twint, NOW);
    TEST_ASSERT_EQUAL_STRING("Bezahlt – CHF 2.00, TWINT CHF 1.50 + Bargeld CHF 0.50 – Transaktion TX-000003",
                             paymentMessage(mixed, "UR1").c_str());
    TEST_ASSERT_EQUAL_STRING("TWINT + Bargeld CHF 0.50, TX-000003",
                             describeSession(*store.findActive("UR1"), NOW).payment.c_str());

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

void test_session_list_order_active_first()
{
    SessionStore store;
    store.openEntry("LU123456", NOW - 3 * HOUR);
    store.openEntry("AG1", NOW - 2 * HOUR);
    store.tryExit("AG1", NOW - 2 * HOUR + MIN); // closed (free)
    store.openEntry("ZH98765", NOW - HOUR);
    store.openEntry("SZ555", NOW - 30 * MIN);
    pay(store, "SZ555", PayMethod::Twint, NOW);
    store.openEntry("AG1", NOW - 10 * MIN); // again, open
    std::vector<const ParkingSession*> list = sessionsForList(store);
    const char* expected[] = {"AG1", "SZ555", "ZH98765", "LU123456", "AG1"};
    TEST_ASSERT_EQUAL_UINT(5, list.size());
    for (size_t i = 0; i < list.size(); ++i)
    {
        TEST_ASSERT_EQUAL_STRING(expected[i], list[i]->plate.c_str());
    }
    TEST_ASSERT_FALSE(isActive(list[4]->status));
}

void test_kasse_page_tf19()
{
    SessionStore store;
    store.openEntry("LU123456", NOW - 80 * MIN);
    std::string html = renderKassePage(store, "lu 123456", Notice{}, NOW, true, "");
    TEST_ASSERT_TRUE(contains(html, "<td>LU123456</td>"));
    TEST_ASSERT_TRUE(contains(html, "<td>15.10.2025 12:46:40</td>"));
    TEST_ASSERT_TRUE(contains(html, "<td>1 h 20 min</td>"));
    TEST_ASSERT_TRUE(contains(html, "<td>CHF 4.00</td>"));
    TEST_ASSERT_TRUE(contains(html, "2 angefangene Stunden"));
    TEST_ASSERT_TRUE(contains(html, "action=\"/kasse/pay\""));
    TEST_ASSERT_TRUE(contains(html, "value=\"cash\""));
    TEST_ASSERT_TRUE(contains(html, "Parkvorgänge"));
    TEST_ASSERT_TRUE(contains(html, "(Admin)"));
}

void test_kasse_list_shows_active_before_closed()
{
    SessionStore store;
    store.openEntry("AG1", NOW - 2 * HOUR);
    store.tryExit("AG1", NOW - 2 * HOUR + MIN);
    store.openEntry("LU123456", NOW - 3 * HOUR);
    std::string html = renderKassePage(store, "", Notice{}, NOW, true, "");
    size_t open = html.find("kz=LU123456\">LU123456");
    size_t closed = html.find("kz=AG1\">AG1");
    TEST_ASSERT_TRUE(open != std::string::npos && closed != std::string::npos);
    TEST_ASSERT_TRUE(open < closed);
}

void test_paid_session_keeps_pay_form_tf20()
{
    SessionStore store;
    store.openEntry("LU123456", NOW - 80 * MIN);
    pay(store, "LU123456", PayMethod::CardOk, NOW);
    Notice notice{"ok", "Bezahlt – CHF 4.00, Karte – Transaktion TX-000001"};
    std::string html = renderKassePage(store, "LU123456", notice, NOW, true, "");
    TEST_ASSERT_TRUE(contains(html, "notice ok\">Bezahlt – CHF 4.00, Karte – Transaktion TX-000001"));
    TEST_ASSERT_TRUE(contains(html, "action=\"/kasse/pay\""));
}

void test_kasse_without_session_tf22()
{
    SessionStore store;
    std::string html = renderKassePage(store, "SZ 555", Notice{}, NOW, true, "");
    TEST_ASSERT_TRUE(contains(html, "Kein offener Parkvorgang für SZ555"));
    TEST_ASSERT_FALSE(contains(html, "/kasse/pay"));
}

void test_kasse_shows_closed_session_without_payment()
{
    SessionStore store;
    store.openEntry("AG1", NOW - 5 * MIN);
    store.tryExit("AG1", NOW);
    std::string html = renderKassePage(store, "AG1", Notice{}, NOW, true, "");
    TEST_ASSERT_TRUE(contains(html, "abgeschlossen (Freizeit)"));
    TEST_ASSERT_FALSE(contains(html, "/kasse/pay"));
}

void test_page_escapes_query_and_flags_invalid_plate()
{
    SessionStore store;
    std::string html = renderKassePage(store, "<script>", Notice{}, NOW, true, "");
    TEST_ASSERT_FALSE(contains(html, "<script>"));
    TEST_ASSERT_TRUE(contains(html, "&lt;script&gt;"));
    TEST_ASSERT_TRUE(contains(html, "Ungültiges Kennzeichen"));
}

void test_page_without_time_offers_no_payment()
{
    SessionStore store;
    store.openEntry("LU123456", NOW - 80 * MIN);
    std::string html = renderKassePage(store, "LU123456", Notice{}, NOW, false, "");
    TEST_ASSERT_TRUE(contains(html, "Uhrzeit nicht synchronisiert"));
    TEST_ASSERT_FALSE(contains(html, "/kasse/pay"));
}

void test_page_without_kasse_shows_fault_only()
{
    SessionStore store;
    std::string html = renderKassePage(store, "LU123456", Notice{}, 0, false, "Kasse nicht erreichbar");
    TEST_ASSERT_TRUE(contains(html, "Kasse nicht erreichbar"));
    TEST_ASSERT_FALSE(contains(html, "Uhrzeit nicht synchronisiert"));
    TEST_ASSERT_FALSE(contains(html, "<h2>Parkvorgänge</h2>"));
    TEST_ASSERT_FALSE(contains(html, "Kein offener Parkvorgang"));
}

void test_tariff_summary_matches_tariff_md()
{
    TEST_ASSERT_EQUAL_STRING("15 Freiminuten; wird die Freizeit überschritten, kostet jede angefangene Stunde ab "
                             "Einfahrt CHF 2.00; Tagesmaximum CHF 20.00 pro angefangene 24 Stunden.",
                             tariffSummary().c_str());
}

void test_tariff_page_us14_tf19()
{
    std::string html = renderTariffPage(Role::Public);
    TEST_ASSERT_TRUE(contains(html, "15 Freiminuten; wird die Freizeit überschritten"));
    // reference values from tariff.md
    TEST_ASSERT_TRUE(contains(html, "<td>0 h 15 min</td><td>CHF 0.00</td>"));        // R-02
    TEST_ASSERT_TRUE(contains(html, "<td>0 h 15 min 1 s</td><td>CHF 2.00</td>"));    // R-03
    TEST_ASSERT_TRUE(contains(html, "<td>1 h 0 min</td><td>CHF 2.00</td>"));         // R-04
    TEST_ASSERT_TRUE(contains(html, "<td>1 h 0 min 1 s</td><td>CHF 4.00</td>"));     // R-05
    TEST_ASSERT_TRUE(contains(html, "<td>1 h 20 min</td><td>CHF 4.00</td>"));        // TF-19
    TEST_ASSERT_TRUE(contains(html, "<td>2 h 30 min</td><td>CHF 6.00</td>"));        // R-06
    TEST_ASSERT_TRUE(contains(html, "<td>10 h 0 min</td><td>CHF 20.00</td>"));       // R-07
    TEST_ASSERT_TRUE(contains(html, "<td>10 h 0 min 1 s</td><td>CHF 20.00</td>"));   // R-08
    TEST_ASSERT_TRUE(contains(html, "<td>24 h 0 min</td><td>CHF 20.00</td>"));       // R-09
    TEST_ASSERT_TRUE(contains(html, "<td>24 h 0 min 1 s</td><td>CHF 22.00</td>"));   // R-10
    TEST_ASSERT_TRUE(contains(html, "<td>48 h 0 min</td><td>CHF 40.00</td>"));       // R-11
    TEST_ASSERT_FALSE(contains(html, "href=\"/kasse\"")); // public nav
}

void test_tariff_linked_from_kasse_tf19()
{
    SessionStore store;
    store.openEntry("LU123456", NOW - 80 * MIN);
    std::string html = renderKassePage(store, "LU123456", Notice{}, NOW, true, "");
    TEST_ASSERT_TRUE(contains(html, "<a href=\"/tarif\">Tarif</a>"));
    TEST_ASSERT_TRUE(contains(html, "<a href=\"/tarif\">Tarif anzeigen</a>"));
}

void test_automat_page_tf19()
{
    SessionStore store;
    store.openEntry("LU 123456", NOW - 80 * MIN);
    std::string html = renderAutomatPage(store, "lu 123456", Notice{}, NOW, true, "", Role::Public);
    TEST_ASSERT_TRUE(contains(html, "<h1>Automat</h1>"));
    TEST_ASSERT_TRUE(contains(html, "1 h 20 min"));
    TEST_ASSERT_TRUE(contains(html, "CHF 4.00"));
    TEST_ASSERT_TRUE(contains(html, "2 angefangene Stunden à CHF 2.00 ab Einfahrt (15 Freiminuten überschritten)"));
    TEST_ASSERT_TRUE(contains(html, "15 Freiminuten; wird die Freizeit überschritten"));
    TEST_ASSERT_TRUE(contains(html, "href=\"/tarif\""));
    TEST_ASSERT_TRUE(contains(html, "action=\"/automat/pay\""));
    TEST_ASSERT_TRUE(contains(html, "value=\"cash\""));
    TEST_ASSERT_FALSE(contains(html, "href=\"/kasse"));
    TEST_ASSERT_TRUE(contains(html, "&#128274; <a href=\"/login\">Anmelden</a>"));
}

void test_automat_hides_other_sessions()
{
    SessionStore store;
    store.openEntry("LU123456", NOW - 80 * MIN);
    store.openEntry("ZH98765", NOW - 150 * MIN);
    std::string html = renderAutomatPage(store, "LU123456", Notice{}, NOW, true, "", Role::Public);
    TEST_ASSERT_FALSE(contains(html, "ZH98765"));
    TEST_ASSERT_FALSE(contains(html, "Parkvorgänge</h2>"));
    std::string empty = renderAutomatPage(store, "", Notice{}, NOW, true, "", Role::Public);
    TEST_ASSERT_FALSE(contains(empty, "LU123456"));
    TEST_ASSERT_FALSE(contains(empty, "ZH98765"));
}

void test_automat_without_session_tf22()
{
    SessionStore store;
    std::string html = renderAutomatPage(store, "SZ 555", Notice{}, NOW, true, "", Role::Public);
    TEST_ASSERT_TRUE(contains(html, "Kein offener Parkvorgang für SZ555"));
    TEST_ASSERT_FALSE(contains(html, "action=\"/automat/pay\""));
}

void test_automat_closed_session_not_shown()
{
    SessionStore store;
    store.openEntry("LU123456", NOW - 80 * MIN);
    pay(store, "LU123456", PayMethod::CardOk, NOW - MIN);
    store.tryExit("LU123456", NOW);
    std::string html = renderAutomatPage(store, "LU123456", Notice{}, NOW, true, "", Role::Public);
    TEST_ASSERT_TRUE(contains(html, "Kein offener Parkvorgang für LU123456"));
    TEST_ASSERT_FALSE(contains(html, "action=\"/automat/pay\""));
}

void test_automat_without_kasse_or_time()
{
    SessionStore store;
    store.openEntry("LU123456", NOW - 80 * MIN);
    std::string down = renderAutomatPage(store, "LU123456", Notice{}, 0, false, "Kasse nicht erreichbar", Role::Public);
    TEST_ASSERT_TRUE(contains(down, "Kasse nicht erreichbar – Parkvorgänge können nicht angezeigt werden"));
    TEST_ASSERT_FALSE(contains(down, "action=\"/automat/pay\""));
    std::string noTime = renderAutomatPage(store, "LU123456", Notice{}, NOW, false, "", Role::Public);
    TEST_ASSERT_TRUE(contains(noTime, "Uhrzeit nicht synchronisiert"));
    TEST_ASSERT_FALSE(contains(noTime, "action=\"/automat/pay\""));
}

void test_kasse_server_page_links_to_pico2()
{
    std::string html = renderKasseServerPage("http://192.168.1.51");
    TEST_ASSERT_TRUE(contains(html, "<a href=\"http://192.168.1.51\">"));
    TEST_ASSERT_FALSE(contains(html, "<nav>"));
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
    RUN_TEST(test_tariff_note_beyond_one_day);
    RUN_TEST(test_paid_session_has_no_running_tariff_note);
    RUN_TEST(test_payment_messages_tf20_tf21_tf22);
    RUN_TEST(test_parse_chf);
    RUN_TEST(test_html_escape);
    RUN_TEST(test_session_list_order_active_first);
    RUN_TEST(test_kasse_page_tf19);
    RUN_TEST(test_kasse_list_shows_active_before_closed);
    RUN_TEST(test_paid_session_keeps_pay_form_tf20);
    RUN_TEST(test_kasse_without_session_tf22);
    RUN_TEST(test_kasse_shows_closed_session_without_payment);
    RUN_TEST(test_page_escapes_query_and_flags_invalid_plate);
    RUN_TEST(test_page_without_time_offers_no_payment);
    RUN_TEST(test_page_without_kasse_shows_fault_only);
    RUN_TEST(test_tariff_summary_matches_tariff_md);
    RUN_TEST(test_tariff_page_us14_tf19);
    RUN_TEST(test_tariff_linked_from_kasse_tf19);
    RUN_TEST(test_automat_page_tf19);
    RUN_TEST(test_automat_hides_other_sessions);
    RUN_TEST(test_automat_without_session_tf22);
    RUN_TEST(test_automat_closed_session_not_shown);
    RUN_TEST(test_automat_without_kasse_or_time);
    RUN_TEST(test_kasse_server_page_links_to_pico2);
    return UNITY_END();
}
