#pragma once

// Tiny line-based command console over the USB-serial link (the same
// cable used to flash the board). This is the only practical way to type
// a WiFi password or a TOTP base32 secret into a device with a single
// button and no keyboard.
//
// Commands (type in the Serial Monitor, 115200 baud, newline-terminated):
//   help                              - list commands
//   wifi set <ssid> <password>        - store STA credentials (NVS)
//   wifi show                         - show stored SSID (not the password)
//   wifi clear                        - erase stored credentials
//   totp add <label> <base32secret>   - add/replace a TOTP account
//   totp del <label>                  - remove a TOTP account
//   totp list                         - list stored labels
namespace serial_console {

void begin();
void poll(); // call every loop()

} // namespace serial_console
