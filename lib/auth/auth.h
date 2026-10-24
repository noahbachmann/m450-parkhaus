#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

// Operator login on Pico 2: a form posts user + password, the browser gets a random session token as a
// cookie. Tokens live in RAM only (a reboot logs everybody out). The random words come from the caller.

extern const char* const OPERATOR_USER; // "betrieb"
extern const char* const SESSION_COOKIE; // "parkhaus_session"
constexpr size_t MAX_LOGINS = 4;          // more browsers: the oldest login is dropped

bool credentialsOk(const std::string& user, const std::string& password, const std::string& expectedPassword);

// Value of a cookie in a Cookie header ("a=1; parkhaus_session=ab12"), empty if missing.
std::string cookieValue(const std::string& cookieHeader, const char* name);

// 32 hex characters from four random words.
std::string makeToken(const uint32_t random[4]);

// Set-Cookie header values.
std::string loginCookie(const std::string& token);
std::string logoutCookie();

class LoginSessions
{
public:
    void add(const std::string& token);
    void remove(const std::string& token);
    // True if the Cookie header carries a valid token.
    bool valid(const std::string& cookieHeader) const;

private:
    std::vector<std::string> tokens;
};
