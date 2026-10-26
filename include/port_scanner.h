#pragma once
#include "ui.h"

// Mode 6: Local network host/port scanner.
//
//   *** Only ever point this at a network and hosts you own or are      ***
//   *** explicitly authorized to test. Scanning networks you don't     ***
//   *** control without permission is illegal in most jurisdictions.    ***
//
// Connects to the WiFi network whose credentials were stored via the
// serial console ("wifi set <ssid> <password>"), ping-sweeps the local
// /24 to find live hosts, then probes each live host against a short
// list of commonly-interesting TCP ports and lists what's open.
namespace port_scanner {

void enter();
void exit();
void loop(ButtonEvent ev);

} // namespace port_scanner
