#include <unity.h>

#include "registration.h"
#include "schranken_view.h"
#include "testdata.h"

namespace
{
    bool contains(const std::string& html, const char* text)
    {
        return html.find(text) != std::string::npos;
    }

    RegistrationList regs;
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
    TEST_ASSERT_TRUE(contains(html, "action=\"/reset\""));
}

void test_camera_page()
{
    LaneView entry{true, "ok", "LU123456 – Willkommen, Einfahrt frei"};
    LaneView exit{false, "err", "Gebühr CHF 6.00 nicht bezahlt – bitte am Automaten bezahlen"};
    std::string html = renderCameraPage(entry, exit);
    TEST_ASSERT_TRUE(contains(html, "action=\"/kamera/einfahrt\""));
    TEST_ASSERT_TRUE(contains(html, "action=\"/kamera/ausfahrt\""));
    TEST_ASSERT_TRUE(contains(html, "name=\"entry\""));
    TEST_ASSERT_TRUE(contains(html, "LU123456 – Willkommen, Einfahrt frei"));
    TEST_ASSERT_TRUE(contains(html, "Gebühr CHF 6.00 nicht bezahlt"));
    TEST_ASSERT_TRUE(contains(html, "Schranke: <strong>offen</strong>"));
    TEST_ASSERT_TRUE(contains(html, "Schranke: <strong>geschlossen</strong>"));
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
    return UNITY_END();
}
