#pragma once
#include <Arduino.h>
#include "ui.h"

// Mode 1: Wi-Fi network scanner.
// Lists nearby APs sorted by signal strength: SSID, channel, RSSI and
// encryption type, flagging open/WEP/WPA(1) networks as weak.
namespace wifi_scanner {

void enter();              // called once when the mode is selected
void exit();               // called once when leaving the mode
void loop(ButtonEvent ev);  // called every frame while active

} // namespace wifi_scanner
