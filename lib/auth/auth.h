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
constexpr size_t TOKEN_WORDS = 4;         // random words per session token (128 bit)

bool credentialsOk(const std::string& user, const std::string& password, const std::string& expectedPassword);

// Value of a cookie in a Cookie header ("a=1; parkhaus_session=ab12"), empty if missing.
std::string cookieValue(const std::string& cookieHeader, const char* name);

// 32 hex characters from TOKEN_WORDS random words.
std::string makeToken(const uint32_t random[TOKEN_WORDS]);

// Appends word as 8 lowercase hex characters ("0000002a").
void appendHex(std::string& out, uint32_t word);

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
