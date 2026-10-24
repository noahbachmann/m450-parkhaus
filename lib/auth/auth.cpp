#include "auth.h"

#include <algorithm>

const char* const OPERATOR_USER = "betrieb";
const char* const SESSION_COOKIE = "parkhaus_session";

namespace
{
    const char* const ADMIN_PAGES[] = {"/kasse", "/registrierung", "/betrieb"};

    std::string trim(const std::string& s)
    {
        size_t begin = s.find_first_not_of(' ');
        if (begin == std::string::npos)
        {
            return "";
        }
        return s.substr(begin, s.find_last_not_of(' ') - begin + 1);
    }
}

bool credentialsOk(const std::string& user, const std::string& password, const std::string& expectedPassword)
{
    return user == OPERATOR_USER && !expectedPassword.empty() && password == expectedPassword;
}

std::string cookieValue(const std::string& cookieHeader, const char* name)
{
    size_t pos = 0;
    while (pos <= cookieHeader.size())
    {
        size_t end = cookieHeader.find(';', pos);
        std::string pair = trim(cookieHeader.substr(pos, end == std::string::npos ? std::string::npos : end - pos));
        size_t eq = pair.find('=');
        if (eq != std::string::npos && pair.compare(0, eq, name) == 0 && eq == std::string(name).size())
        {
            return pair.substr(eq + 1);
        }
        if (end == std::string::npos)
        {
            break;
        }
        pos = end + 1;
    }
    return "";
}

std::string makeToken(const uint32_t random[4])
{
    static const char HEX[] = "0123456789abcdef";
    std::string token;
    for (int i = 0; i < 4; ++i)
    {
        for (int shift = 28; shift >= 0; shift -= 4)
        {
            token += HEX[(random[i] >> shift) & 0xF];
        }
    }
    return token;
}

std::string loginCookie(const std::string& token)
{
    return std::string(SESSION_COOKIE) + "=" + token + "; Path=/; HttpOnly; SameSite=Strict";
}

std::string logoutCookie()
{
    return std::string(SESSION_COOKIE) + "=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict";
}

std::string loginTarget(const std::string& next)
{
    for (const char* page : ADMIN_PAGES)
    {
        if (next == page)
        {
            return next;
        }
    }
    return "/betrieb";
}

void LoginSessions::add(const std::string& token)
{
    if (token.empty())
    {
        return;
    }
    if (tokens.size() >= MAX_LOGINS)
    {
        tokens.erase(tokens.begin());
    }
    tokens.push_back(token);
}

void LoginSessions::remove(const std::string& token)
{
    tokens.erase(std::remove(tokens.begin(), tokens.end(), token), tokens.end());
}

bool LoginSessions::valid(const std::string& cookieHeader) const
{
    std::string token = cookieValue(cookieHeader, SESSION_COOKIE);
    return !token.empty() && std::find(tokens.begin(), tokens.end(), token) != tokens.end();
}
