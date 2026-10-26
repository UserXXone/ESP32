#pragma once
#include "ui.h"

// Mode 8: Deauth attack (active, ESP32Marauder-style).
//
//   *** Transmits real 802.11 deauthentication frames. Only point this  ***
//   *** at an access point / network you own or are explicitly          ***
//   *** authorized to test (pentest engagement, CTF, your own lab AP).  ***
//   *** Using it against networks you don't control is illegal in most  ***
//   *** jurisdictions (unauthorized access / wireless interference).    ***
//
// Flow: scans for nearby APs, you pick one (SHORT=next, LONG=lock target
// and start), then broadcasts deauth frames to all its clients on that
// AP's channel until stopped (SHORT=stop, VLONG=menu as usual).
namespace deauth_attack {

void enter();
void exit();
void loop(ButtonEvent ev);

} // namespace deauth_attack
