#pragma once
#include <Arduino.h>

// Wraps Preferences (NVS) for the two things that need persisted,
// user-supplied secrets: WiFi STA credentials (for the port scanner /
// NTP sync) and TOTP accounts. Entered via the serial console, never
// hardcoded, never logged back out in full.

struct TotpAccount {
    String label;
    String base32secret;
};

namespace storage {

void begin();

// WiFi credentials (single profile is enough for this device).
void setWifi(const String &ssid, const String &password);
bool getWifi(String &ssid, String &password);
bool hasWifi();
void clearWifi();

// TOTP accounts (small fixed-size list, simplicity over scale).
static const int TOTP_MAX_ACCOUNTS = 8;
int totpCount();
bool totpGet(int index, TotpAccount &out);
bool totpAdd(const String &label, const String &base32secret);
bool totpRemove(const String &label);

} // namespace storage
