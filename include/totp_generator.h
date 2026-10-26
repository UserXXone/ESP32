#pragma once
#include "ui.h"

// Mode 7: TOTP (RFC 6238) code generator, offline-authenticator style.
// Accounts (label + base32 secret) are entered once via the serial
// console ("totp add <label> <secret>") and stored in NVS. Needs an
// accurate clock, so on first use (or on request) it briefly joins the
// stored WiFi network to fetch time over NTP, then disconnects and runs
// off its internal clock for the rest of the session.
namespace totp_generator {

void enter();
void exit();
void loop(ButtonEvent ev);

} // namespace totp_generator
