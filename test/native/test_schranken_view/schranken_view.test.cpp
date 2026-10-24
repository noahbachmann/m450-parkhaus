#include <unity.h>

#include "auth.h"
#include "payment.h"
#include "registration.h"
#include "schranken_view.h"
#include "testdata.h"

namespace
{
    bool contains(const std::string& html, const char* text)
    {
        return html.find(text) != std::string::npos;
    }

    constexpr int64_t NOW = 1760530000; // 15.10.2025 14:06:40 Zurich (CEST)
    constexpr int64_t MIN = 60;

    RegistrationList regs;

    std::string camera(const LaneView& entry, const LaneView& exit, Role role = Role::Public)
    {
        return renderCameraPage(entry, exit, KasseSnapshot{}, regs, role);
    }
}

void setUp()
{
    loadDefaultRegistrations(regs);
}

void tearDown()
{
}

void test_register_tf01()
{
    FormResult r = handleRegistrationForm(regs, "Test Kunde C", "NW 654321");
    TEST_ASSERT_TRUE(r.changed);
    TEST_ASSERT_EQUAL_STRING("ok", r.notice.cls);
    TEST_ASSERT_EQUAL_STRING("Kennzeichen registriert: NW654321", r.notice.text.c_str());
    std::string html = renderRegistrationPage(regs, r.notice);
    TEST_ASSERT_TRUE(contains(html, "<td>NW654321</td><td>Test Kunde C</td>"));
}

void test_invalid_plates_rejected_tf02()
{
    const char* invalid[] = {"", "LU-12#45", "LU 1234567", "L 123"};
    for (const char* kz : invalid)
    {
        FormResult r = handleRegistrationForm(regs, "Test Kunde C", kz);
        TEST_ASSERT_FALSE(r.changed);
        TEST_ASSERT_EQUAL_STRING("Ungültiges Kennzeichen – erwartet 2 Buchstaben und 1–6 Ziffern",
                                 r.notice.text.c_str());
    }
    TEST_ASSERT_EQUAL_UINT(4, regs.all().size());
    TEST_ASSERT_TRUE(handleRegistrationForm(regs, "Test Kunde C", "UR 1").changed);
}

void test_duplicates_rejected_tf03()
{
    FormResult a = handleRegistrationForm(regs, "Test Kunde B", "LU 123456");
    FormResult b = handleRegistrationForm(regs, "Test Kunde B", "lu123456");
    TEST_ASSERT_EQUAL_STRING("Kennzeichen bereits registriert", a.notice.text.c_str());
    TEST_ASSERT_EQUAL_STRING("Kennzeichen bereits registriert", b.notice.text.c_str());
    TEST_ASSERT_FALSE(a.changed || b.changed);
    TEST_ASSERT_EQUAL_STRING("Test Kunde A", regs.all()[0].name.c_str());
}

void test_name_required()
{
    FormResult r = handleRegistrationForm(regs, "   ", "UR 1");
    TEST_ASSERT_FALSE(r.changed);
    TEST_ASSERT_EQUAL_STRING("Bitte Namen eingeben (1–40 Zeichen)", r.notice.text.c_str());
}

void test_delete()
{
    FormResult r = handleDeleteForm(regs, "sz 555");
    TEST_ASSERT_TRUE(r.changed);
    TEST_ASSERT_EQUAL_STRING("Registrierung SZ555 gelöscht", r.notice.text.c_str());
    FormResult again = handleDeleteForm(regs, "SZ555");
    TEST_ASSERT_FALSE(again.changed);
    TEST_ASSERT_EQUAL_STRING("Kennzeichen SZ555 nicht registriert", again.notice.text.c_str());
}

void test_registration_page_escapes_names()
{
    regs.add("<b>Evil</b>", "UR 1");
    std::string html = renderRegistrationPage(regs, Notice{});
    TEST_ASSERT_FALSE(contains(html, "<b>Evil</b>"));
    TEST_ASSERT_TRUE(contains(html, "&lt;b&gt;Evil&lt;/b&gt;"));
    TEST_ASSERT_TRUE(contains(html, "action=\"/registrierung/delete\""));
    TEST_ASSERT_TRUE(contains(html, "(Admin)"));
}

void test_camera_page()
{
    LaneView entry{"offen", "ok", "LU123456 – Willkommen, Einfahrt frei", "", "", ""};
    LaneView exit{"geschlossen", "err", "Gebühr CHF 6.00 nicht bezahlt – bitte hier oder an der Kasse bezahlen", "", "", ""};
    std::string html = camera(entry, exit);
    TEST_ASSERT_TRUE(contains(html, "action=\"/kamera/einfahrt\""));
    TEST_ASSERT_TRUE(contains(html, "action=\"/kamera/ausfahrt\""));
    TEST_ASSERT_TRUE(contains(html, "name=\"entry\""));
    TEST_ASSERT_TRUE(contains(html, "LU123456 – Willkommen, Einfahrt frei"));
    TEST_ASSERT_TRUE(contains(html, "Gebühr CHF 6.00 nicht bezahlt"));
    TEST_ASSERT_TRUE(contains(html, "Schranke: <strong>offen</strong>"));
    TEST_ASSERT_TRUE(contains(html, "Schranke: <strong>geschlossen</strong>"));
    TEST_ASSERT_FALSE(contains(html, "min belegt"));
    TEST_ASSERT_TRUE(contains(html, "name=\"lane\" value=\"entry\"><button type=\"submit\">Ruftaste</button>"));
    TEST_ASSERT_TRUE(contains(html, "name=\"lane\" value=\"exit\"><button type=\"submit\">Ruftaste</button>"));
    TEST_ASSERT_TRUE(contains(html, "Fahrzeug wendet"));
    TEST_ASSERT_FALSE(contains(html, "/kamera/bezahlen"));
    TEST_ASSERT_FALSE(contains(html, "/kamera/durchfahren"));
    TEST_ASSERT_TRUE(contains(html, "Kasse nicht erreichbar – Parkvorgänge können nicht angezeigt werden"));
}

void test_drive_through_button_only_while_open()
{
    LaneView entry{"offen", "ok", "LU123456 – Willkommen, Einfahrt frei", "", "", ""};
    entry.canPass = true;
    std::string html = camera(entry, LaneView{});
    TEST_ASSERT_TRUE(contains(html, "action=\"/kamera/durchfahren\" class=\"inline\"><input type=\"hidden\" "
                                    "name=\"lane\" value=\"entry\"><button type=\"submit\"><strong>Durchfahren"));
    TEST_ASSERT_FALSE(contains(html, "value=\"exit\"><button type=\"submit\"><strong>Durchfahren"));
}

void test_loop_toggle_tf13()
{
    LaneView entry{"offen", "ok", "LU123456 – Willkommen, Einfahrt frei", "", "", ""};
    entry.canPass = true;
    entry.canHold = true;
    LaneView exit{"schliesst", "ok", "Fahrzeug durchgefahren – Schranke schliesst", "", "", ""};
    exit.loopHeld = true;
    std::string html = camera(entry, exit);
    TEST_ASSERT_TRUE(contains(html, "action=\"/kamera/schleife\" class=\"inline\"><input type=\"hidden\" name=\"lane\" "
                                    "value=\"entry\"><input type=\"hidden\" name=\"held\" value=\"1\"><button "
                                    "type=\"submit\">Fahrzeug bleibt stehen</button>"));
    TEST_ASSERT_TRUE(contains(html, "value=\"exit\"><input type=\"hidden\" name=\"held\" value=\"0\"><button "
                                    "type=\"submit\">Fahrzeug fährt weiter</button>"));
    TEST_ASSERT_TRUE(contains(html, "Schranke: <strong>schliesst</strong> – Sicherungsschleife: <strong>belegt</strong>"));
    TEST_ASSERT_TRUE(contains(html, "value=\"entry\"><button type=\"submit\"><strong>Durchfahren"));
    TEST_ASSERT_FALSE(contains(html, "value=\"exit\"><button type=\"submit\"><strong>Durchfahren"));
    TEST_ASSERT_FALSE(contains(html, "value=\"exit\"><input type=\"hidden\" name=\"held\" value=\"1\">"));
}

void test_no_loop_toggle_while_closed()
{
    std::string html = camera(LaneView{}, LaneView{});
    TEST_ASSERT_FALSE(contains(html, "/kamera/schleife"));
    TEST_ASSERT_TRUE(contains(html, "Schranke: <strong>geschlossen</strong></p>"));
}

void test_nav_public_hides_admin_pages()
{
    std::string html = camera(LaneView{}, LaneView{});
    TEST_ASSERT_TRUE(contains(html, "<a href=\"/kamera\">Kamerasimulation</a><a href=\"/tarif\">Tarif</a>"));
    TEST_ASSERT_FALSE(contains(html, "href=\"/kasse\""));
    TEST_ASSERT_FALSE(contains(html, "href=\"/registrierung\""));
    TEST_ASSERT_FALSE(contains(html, "href=\"/betrieb\""));
    TEST_ASSERT_TRUE(contains(html, "&#128274; <a href=\"/login\">Anmelden</a>"));
    TEST_ASSERT_FALSE(contains(html, "/logout"));
    TEST_ASSERT_FALSE(contains(html, "/sim"));
    TEST_ASSERT_FALSE(contains(html, "Automat"));
}

void test_nav_admin_shows_badge_and_admin_pages()
{
    std::string html = camera(LaneView{}, LaneView{}, Role::Admin);
    TEST_ASSERT_TRUE(contains(html, "<a href=\"/kasse\">Kasse</a><a href=\"/registrierung\">Registrierung</a>"
                                    "<a href=\"/betrieb\">Betrieb</a>"));
    TEST_ASSERT_TRUE(contains(html, "class=\"who admin\">&#128100; betrieb (Admin)"));
    TEST_ASSERT_TRUE(contains(html, "action=\"/logout\""));
    TEST_ASSERT_FALSE(contains(html, "href=\"/login\""));
}

void test_camera_lists_sessions_active_first_and_suggests_plates()
{
    KasseSnapshot kasse;
    kasse.error.clear();
    kasse.now = NOW;
    kasse.sessions.openEntry("AG1", NOW - 60 * MIN);
    kasse.sessions.tryExit("AG1", NOW - 59 * MIN);
    kasse.sessions.openEntry("UR1", NOW - 20 * MIN);
    kasse.sessions.openEntry("ZH98765", NOW - 150 * MIN);
    pay(kasse.sessions, "ZH98765", PayMethod::Twint, NOW);
    std::string html = renderCameraPage(LaneView{}, LaneView{}, kasse, regs, Role::Public);
    size_t paid = html.find("<td>ZH98765</td>");
    size_t open = html.find("<td>UR1</td>");
    size_t closed = html.find("<td>AG1</td>");
    TEST_ASSERT_TRUE(paid != std::string::npos && open != std::string::npos && closed != std::string::npos);
    TEST_ASSERT_TRUE(paid < open && open < closed);
    TEST_ASSERT_TRUE(contains(html, "<td>CHF 2.00</td><td>offen (unbezahlt)</td>"));
    TEST_ASSERT_TRUE(contains(html, "<td>abgeschlossen (Freizeit)</td>"));
    // registered plates + plates in the car park, once each, sorted
    TEST_ASSERT_TRUE(contains(html, "<datalist id=\"plates\"><option value=\"AG1\"><option value=\"LU123456\">"
                                    "<option value=\"SZ555\"><option value=\"UR1\"><option value=\"ZH98765\">"
                                    "</datalist>"));
    TEST_ASSERT_TRUE(contains(html, "list=\"plates\""));
}

void test_login_page()
{
    std::string html = renderLoginPage(Notice{"err", "Benutzer oder Passwort falsch"}, "/kasse");
    TEST_ASSERT_TRUE(contains(html, "action=\"/login\""));
    TEST_ASSERT_TRUE(contains(html, "name=\"next\" value=\"/kasse\""));
    TEST_ASSERT_TRUE(contains(html, "type=\"password\""));
    TEST_ASSERT_TRUE(contains(html, "notice err\">Benutzer oder Passwort falsch"));
}

void test_exit_terminal_payment_form_tf28()
{
    LaneView exit{"geschlossen", "err", "Gebühr CHF 6.00 nicht bezahlt – bitte hier oder an der Kasse bezahlen", "", "ZH98765",
                  "CHF 6.00"};
    std::string html = camera(LaneView{}, exit);
    TEST_ASSERT_TRUE(contains(html, "action=\"/kamera/bezahlen\""));
    TEST_ASSERT_TRUE(contains(html, "Gebühr CHF 6.00 für ZH98765 – hier bezahlen"));
    TEST_ASSERT_TRUE(contains(html, "value=\"card_declined\""));
    TEST_ASSERT_TRUE(contains(html, "value=\"twint\""));
    TEST_ASSERT_FALSE(contains(html, "value=\"cash\""));
}

void test_camera_page_shows_loop_fault_tf15()
{
    LaneView entry{"offen", "", "", "Sicherungsschleife Einfahrt länger als 5 min belegt", "", ""};
    std::string html = camera(entry, LaneView{});
    TEST_ASSERT_TRUE(contains(html, "notice err\">Sicherungsschleife Einfahrt länger als 5 min belegt"));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_register_tf01);
    RUN_TEST(test_invalid_plates_rejected_tf02);
    RUN_TEST(test_duplicates_rejected_tf03);
    RUN_TEST(test_name_required);
    RUN_TEST(test_delete);
    RUN_TEST(test_registration_page_escapes_names);
    RUN_TEST(test_camera_page);
    RUN_TEST(test_camera_page_shows_loop_fault_tf15);
    RUN_TEST(test_exit_terminal_payment_form_tf28);
    RUN_TEST(test_drive_through_button_only_while_open);
    RUN_TEST(test_loop_toggle_tf13);
    RUN_TEST(test_no_loop_toggle_while_closed);
    RUN_TEST(test_nav_public_hides_admin_pages);
    RUN_TEST(test_nav_admin_shows_badge_and_admin_pages);
    RUN_TEST(test_camera_lists_sessions_active_first_and_suggests_plates);
    RUN_TEST(test_login_page);
    return UNITY_END();
}
