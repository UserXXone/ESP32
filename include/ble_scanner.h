#pragma once
#include "ui.h"

// Mode 4: BLE scanner.
// Lists nearby BLE advertisers (address, RSSI, name when present), flags
// Apple "Find My" offline-finding broadcasts (used by AirTags and other
// Find-My-network accessories while separated from their owner -- a
// heuristic signature, not a certainty) and raises an alert when the rate
// of distinct/rotating addresses spikes, which is how BLE MAC-spam /
// fake-pairing-request flood attacks look to a passive scanner.
namespace ble_scanner {

void enter();
void exit();
void loop(ButtonEvent ev);

} // namespace ble_scanner
