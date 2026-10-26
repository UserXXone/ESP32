#pragma once
#include "ui.h"

// Mode 9: Beacon spam (active, ESP32Marauder-style).
//
//   *** Broadcasts fake 802.11 beacon frames advertising made-up SSIDs. ***
//   *** For authorized demos/testing only (e.g. showing how WIDS reacts ***
//   *** to rogue-AP floods, or stress-testing your own scanning tools). ***
//   *** Don't run this where it could confuse or annoy people who       ***
//   *** aren't part of the test.                                       ***
//
// Cycles a built-in list of SSIDs across the 2.4GHz channels, each
// advertised from a random, locally-administered MAC.
namespace beacon_spam {

void enter();
void exit();
void loop(ButtonEvent ev);

} // namespace beacon_spam
