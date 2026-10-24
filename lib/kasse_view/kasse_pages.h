#pragma once

#include <stdint.h>
#include <string>

#include "kasse_view.h"
#include "session.h"

// Pages served by Pico 2 with the sessions read from Pico 1 (see kasse_client.h).

// /tarif: the valid tariff (US-14 AK3) with example fees, all generated from lib/tariff.
std::string renderTariffPage(Role role);

// /kasse (operator): search any plate (latest session, also closed), pay, list all sessions.
// Notice: result of the last POST, shown above the search form. now / timeOk: Pico 1 clock;
// reachable = false: Pico 1 didn't answer, only the notice and the search form are shown.
std::string renderKassePage(const SessionStore& store, const std::string& query, const Notice& notice, int64_t now,
                            bool timeOk, bool reachable);

// GET / on Pico 1: it only serves the API; the pages are on Pico 2 at uiUrl.
std::string renderKasseServerPage(const std::string& uiUrl);
