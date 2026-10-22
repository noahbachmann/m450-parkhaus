#pragma once

#include <stdint.h>
#include <string>

#include "session.h"

enum class Page
{
    Kasse,
    Automat,
};

struct Notice
{
    const char* cls = ""; // "ok", "info" or "err"; empty = no notice
    std::string text;
};

// Base path of the page: "/kasse" or "/automat".
const char* pagePath(Page page);

// Kasse: search any plate (latest session, also closed), pay, list all sessions, test-data reset.
// Automat: only open/paid sessions, pay.
// Notice: result of the last POST, shown above the search form.
std::string renderPage(Page page, const SessionStore& store, const std::string& query, const Notice& notice,
                       int64_t now, bool timeOk);
