#pragma once
#include "ui.h"

// Mode 3: Evil twin / rogue AP detector.
// Repeatedly scans and builds a baseline of SSID -> {BSSID, encryption}
// pairs seen over time. Flags an SSID as suspicious when:
//   - the same SSID is seen broadcast with two different encryption
//     types (classic evil twin: clone the name, drop the security), or
//   - a new BSSID shows up under an SSID that previously had a stable,
//     different set of BSSIDs (a second transmitter claiming the same
//     identity).
// Heuristic, not proof -- legitimate multi-AP/mesh deployments can also
// show multiple BSSIDs per SSID (same encryption), which is not flagged.
namespace eviltwin_detector {

void enter();
void exit();
void loop(ButtonEvent ev);

} // namespace eviltwin_detector
