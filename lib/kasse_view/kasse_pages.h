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
// kasseError non-empty (see KasseSnapshot::error): no session list, only the notices and the search form.
std::string renderKassePage(const SessionStore& store, const std::string& query, const Notice& notice, int64_t now,
                            bool timeOk, const std::string& kasseError);

// Radio buttons «Karte OK», «Karte abgelehnt», «TWINT» (+ «Bargeld»), field name "method".
void appendPaymentMethods(std::string& html, bool withCash);

// Session table, order of sessionsForList. kasseLinks (operator Kasse): plates link to /kasse?kz= and a «Zahlung»
// column is added; otherwise (camera page) open sessions are marked «(unbezahlt)».
void appendSessionTable(std::string& html, const SessionStore& store, int64_t now, bool kasseLinks);

// GET / on Pico 1: it only serves the API; the pages are on Pico 2 at uiUrl.
std::string renderKasseServerPage(const std::string& uiUrl);
