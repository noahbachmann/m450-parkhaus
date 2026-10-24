#include <unity.h>

#include "auth.h"

void setUp()
{
}

void tearDown()
{
}

void test_credentials()
{
    TEST_ASSERT_TRUE(credentialsOk("betrieb", "parkhaus", "parkhaus"));
    TEST_ASSERT_FALSE(credentialsOk("betrieb", "falsch", "parkhaus"));
    TEST_ASSERT_FALSE(credentialsOk("Betrieb", "parkhaus", "parkhaus"));
    TEST_ASSERT_FALSE(credentialsOk("admin", "parkhaus", "parkhaus"));
    TEST_ASSERT_FALSE(credentialsOk("betrieb", "", "")); // no password configured: nobody gets in
}

void test_cookie_value()
{
    TEST_ASSERT_EQUAL_STRING("ab12", cookieValue("parkhaus_session=ab12", SESSION_COOKIE).c_str());
    TEST_ASSERT_EQUAL_STRING("ab12", cookieValue("a=1; parkhaus_session=ab12; b=2", SESSION_COOKIE).c_str());
    TEST_ASSERT_EQUAL_STRING("", cookieValue("xparkhaus_session=ab12", SESSION_COOKIE).c_str());
    TEST_ASSERT_EQUAL_STRING("", cookieValue("parkhaus_sessionx=ab12", SESSION_COOKIE).c_str());
    TEST_ASSERT_EQUAL_STRING("", cookieValue("", SESSION_COOKIE).c_str());
}

void test_token_and_cookies()
{
    const uint32_t words[4] = {0x01234567, 0x89abcdef, 0, 0xffffffff};
    TEST_ASSERT_EQUAL_STRING("0123456789abcdef00000000ffffffff", makeToken(words).c_str());
    TEST_ASSERT_EQUAL_STRING("parkhaus_session=t1; Path=/; HttpOnly; SameSite=Strict", loginCookie("t1").c_str());
    TEST_ASSERT_EQUAL_STRING("parkhaus_session=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict",
                             logoutCookie().c_str());
}

void test_login_sessions()
{
    LoginSessions logins;
    TEST_ASSERT_FALSE(logins.valid("parkhaus_session=t1"));
    logins.add("t1");
    logins.add("t2"); // second browser
    TEST_ASSERT_TRUE(logins.valid("parkhaus_session=t1"));
    TEST_ASSERT_TRUE(logins.valid("x=1; parkhaus_session=t2"));
    TEST_ASSERT_FALSE(logins.valid("parkhaus_session=t3"));
    TEST_ASSERT_FALSE(logins.valid("parkhaus_session="));
    logins.remove("t1");
    TEST_ASSERT_FALSE(logins.valid("parkhaus_session=t1"));
    TEST_ASSERT_TRUE(logins.valid("parkhaus_session=t2"));
}

void test_oldest_login_dropped()
{
    LoginSessions logins;
    for (size_t i = 0; i <= MAX_LOGINS; ++i)
    {
        logins.add("t" + std::to_string(i));
    }
    TEST_ASSERT_FALSE(logins.valid("parkhaus_session=t0"));
    TEST_ASSERT_TRUE(logins.valid("parkhaus_session=t" + std::to_string(MAX_LOGINS)));
    logins.add("");
    TEST_ASSERT_FALSE(logins.valid("parkhaus_session="));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_credentials);
    RUN_TEST(test_cookie_value);
    RUN_TEST(test_token_and_cookies);
    RUN_TEST(test_login_sessions);
    RUN_TEST(test_oldest_login_dropped);
    return UNITY_END();
}
