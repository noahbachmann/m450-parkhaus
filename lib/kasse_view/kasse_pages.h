#pragma once

#include <stdint.h>
#include <string>

#include "kasse_view.h"
#include "session.h"

enum class Page
{
    Kasse,
    Automat,
};

// Navigation links of the Pico 1 pages (Kasse, Automat, Tarif).
extern const char* const KASSE_NAV;

// /tarif: the valid tariff (US-14 AK3) with example fees, all generated from lib/tariff.
std::string renderTariffPage();

// Base path of the page: "/kasse" or "/automat".
const char* pagePath(Page page);

// Kasse: search any plate (latest session, also closed), pay, list all sessions, test-data reset.
// Automat: only open/paid sessions, pay.
// Notice: result of the last POST, shown above the search form.
std::string renderPage(Page page, const SessionStore& store, const std::string& query, const Notice& notice,
                       int64_t now, bool timeOk);
