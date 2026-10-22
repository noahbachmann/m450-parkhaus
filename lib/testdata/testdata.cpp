#include "testdata.h"

void loadDefaultRegistrations(RegistrationList& list)
{
    list.clear();
    list.add("Test Kunde A", "LU 123456");
    list.add("Test Kunde B", "ZH 98765");
    list.add("Test Kunde D", "AG 1");
    list.add("Test Kunde E", "SZ 555");
}

void loadDefaultSessions(SessionStore& store)
{
    store.restore({}, 1);
}
