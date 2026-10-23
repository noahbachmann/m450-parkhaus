#pragma once

#include <string>

#include "html.h"
#include "registration.h"

// Pico 2 pages: registration (/registrierung) and camera simulation (/kamera).

// «Kennzeichen registriert», «Kennzeichen bereits registriert», …
std::string registrationMessage(RegistrationResult result);

// POST /registrierung/add: fields name, kz.
FormResult handleRegistrationForm(RegistrationList& list, const std::string& name, const std::string& kz);
// POST /registrierung/delete: field kz.
FormResult handleDeleteForm(RegistrationList& list, const std::string& kz);

std::string renderRegistrationPage(const RegistrationList& list, const Notice& notice);

struct LaneView
{
    const char* barrier = "geschlossen"; // barrierStateLabel()
    const char* cls = "";                // class of the last terminal text: "ok", "err" or empty
    std::string terminal;                // last text shown on the lane's terminal
    std::string fault;                   // loop fault message; empty if none
};

std::string renderCameraPage(const LaneView& entry, const LaneView& exit);
