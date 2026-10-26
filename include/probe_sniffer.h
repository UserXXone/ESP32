#pragma once
#include "ui.h"

// Mode 10: Probe request sniffer (passive, ESP32Marauder-style).
// Devices looking for WiFi broadcast "probe request" frames naming
// networks they remember -- this lists which nearby devices (by MAC)
// are probing for which SSIDs. Passive/receive-only, same recon
// technique Marauder's "Sniff probe" mode uses.
namespace probe_sniffer {

void enter();
void exit();
void loop(ButtonEvent ev);

} // namespace probe_sniffer
