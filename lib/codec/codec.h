#pragma once

#include <string>

#include "registration.h"
#include "session.h"

// File formats for LittleFS. Parsers return false on malformed or invalid input and leave the target unchanged.
//   registrations: {"registrations":[{"name":"Test Kunde A","kz":"LU123456"}]}
//   sessions:      {"nextTx":3,"sessions":[{"kz":"LU123456","entry":1760530000,"exit":0,"amount":400,
//                   "cash":0,"method":"card","tx":"TX-000001","status":"paid"}]}

std::string registrationsToJson(const RegistrationList& list);
bool registrationsFromJson(const std::string& json, RegistrationList& list);

std::string sessionsToJson(const SessionStore& store);
bool sessionsFromJson(const std::string& json, SessionStore& store);

// Status names as used in the files and the API: "open", "paid", "closed", "closed_free".
const char* sessionStatusName(SessionStatus status);
