#pragma once
#include "ui.h"

// Mode 2: Deauthentication / disassociation flood detector.
// Passively sniffs 802.11 management frames (promiscuous mode, no
// injection) across all channels and alarms when the rate of
// deauth/disassoc frames crosses a threshold -- the classic signature of
// a deauth attack (e.g. aireplay-ng, ESP32 deauther boards) against
// nearby clients/APs. Defensive only: it never transmits deauth frames.
namespace deauth_detector {

void enter();
void exit();
void loop(ButtonEvent ev);

} // namespace deauth_detector
