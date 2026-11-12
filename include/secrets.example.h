#pragma once

// Copy to include/secrets.h (gitignored) and fill in. Overview of all dev/test credentials: README.md,
// "Zugangsdaten für Entwicklung und Test".

#define WIFI_SSID "test-wlan"
#define WIFI_PASSWORD "changeme"

// Fixed network of the setup; do not change.
#define KASSE_IP 10, 178, 56, 156
#define SCHRANKEN_IP 10, 178, 56, 157
#define GATEWAY_IP 10, 178, 56, 50
#define SUBNET_MASK 255, 255, 255, 0

// Password of the operator view /betrieb on Pico 2 (user «betrieb»)
#define OPERATOR_PASSWORD "parkhaus"

// Shared key: Pico 2 sends it in X-Api-Key, Pico 1 accepts writes only with it (and only from SCHRANKEN_IP).
// Same value on both Picos; use a long random string, e.g. openssl rand -hex 16.
#define KASSE_API_KEY "change-me-to-a-long-random-string"
