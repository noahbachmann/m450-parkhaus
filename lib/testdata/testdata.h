#pragma once

#include "registration.h"
#include "session.h"

// Test-data reset (test-concept.md, KZ table): pre-registered plates KZ-01, KZ-02, KZ-08, KZ-10.
void loadDefaultRegistrations(RegistrationList& list);
// No sessions, transaction counter back to 1.
void loadDefaultSessions(SessionStore& store);
