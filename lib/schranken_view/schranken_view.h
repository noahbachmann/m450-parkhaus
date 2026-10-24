#pragma once

#include <string>

#include "html.h"
#include "kasse_client.h"
#include "registration.h"

// Pico 2 pages: registration (/registrierung), camera simulation (/kamera), login (/login).

// «Kennzeichen registriert», «Kennzeichen bereits registriert», …
std::string registrationMessage(RegistrationResult result);

// POST /registrierung/add: fields name, kz.
FormResult handleRegistrationForm(RegistrationList& list, const std::string& name, const std::string& kz);
// POST /registrierung/delete: field kz.
FormResult handleDeleteForm(RegistrationList& list, const std::string& kz);

// Operator only.
std::string renderRegistrationPage(const RegistrationList& list, const Notice& notice);

// next: admin page to return to after login (see loginTarget), passed through as a hidden field.
std::string renderLoginPage(const Notice& notice, const std::string& next);

struct LaneView
{
    const char* barrier = "geschlossen"; // barrierStateLabel()
    const char* cls = "";                // class of the last terminal text: "ok", "err" or empty
    std::string terminal;                // last text shown on the lane's terminal
    std::string fault;                   // loop fault message; empty if none
    std::string payPlate;                // exit: plate with an unpaid fee -> payment form; empty = none
    std::string payFee;                  // "CHF 6.00"
    bool canPass = false;                // barrier open, loop free: «Durchfahren» offered
    bool canHold = false;                // barrier up: «Fahrzeug bleibt stehen» offered
    bool loopHeld = false;               // «Fahrzeug bleibt stehen» pressed: only «Fahrzeug fährt weiter» offered
};

// Lanes on top, below the sessions from Pico 1 (active first) as a testing aid. The plate fields suggest
// registered plates and plates in the car park.
std::string renderCameraPage(const LaneView& entry, const LaneView& exit, const KasseSnapshot& kasse,
                             const RegistrationList& registrations, Role role);
