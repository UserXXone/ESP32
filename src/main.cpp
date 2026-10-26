// ESP32-S3 Cyber-Security Toolkit
// ---------------------------------------------------------------------------
// Hardware: ESP32-S3 WROOM dev board + 0.96" SSD1306 OLED (I2C). No other
// peripherals. Navigation uses the board's built-in BOOT button (GPIO0);
// text input (WiFi credentials, TOTP secrets) uses the USB-serial console
// that's already there for flashing -- see serial_console.h for commands.
//
//   *** Defensive / educational use only. The WiFi and BLE scanners,     ***
//   *** the deauth/evil-twin/BLE-spam detectors and the packet monitor   ***
//   *** are all passive (receive-only) -- they never transmit deauth or  ***
//   *** other disruptive frames. The port scanner actively probes hosts  ***
//   *** and must only ever be pointed at your own network.               ***
//
// Single-button convention (see ui.h for details):
//   SHORT  -> cycle menu / scroll a list
//   LONG   -> select / enter mode / mode-specific action (rescan, lock...)
//   VLONG  (>=2.5s) -> always back to the main menu
// ---------------------------------------------------------------------------
#include <Arduino.h>
#include "ui.h"
#include "storage.h"
#include "serial_console.h"

#include "wifi_scanner.h"
#include "deauth_detector.h"
#include "eviltwin_detector.h"
#include "ble_scanner.h"
#include "packet_monitor.h"
#include "port_scanner.h"
#include "totp_generator.h"
#include "deauth_attack.h"
#include "beacon_spam.h"
#include "probe_sniffer.h"

enum class Mode {
    MENU,
    WIFI_SCAN,
    DEAUTH,
    EVIL_TWIN,
    BLE_SCAN,
    PKT_MONITOR,
    PORT_SCAN,
    TOTP,
    DEAUTH_ATTACK,
    BEACON_SPAM,
    PROBE_SNIFF,
    MODE_COUNT
};

struct MenuItem {
    const char *name;
    void (*enterFn)();
    void (*exitFn)();
    void (*loopFn)(ButtonEvent);
};

static const MenuItem menuItems[] = {
    {"WiFi Scanner",   wifi_scanner::enter,      wifi_scanner::exit,      wifi_scanner::loop},
    {"Deauth Detect",  deauth_detector::enter,   deauth_detector::exit,   deauth_detector::loop},
    {"Evil Twin Det.", eviltwin_detector::enter, eviltwin_detector::exit, eviltwin_detector::loop},
    {"BLE Scanner",    ble_scanner::enter,       ble_scanner::exit,       ble_scanner::loop},
    {"Packet Monitor", packet_monitor::enter,    packet_monitor::exit,    packet_monitor::loop},
    {"Port Scanner",   port_scanner::enter,      port_scanner::exit,     port_scanner::loop},
    {"TOTP Generator", totp_generator::enter,    totp_generator::exit,   totp_generator::loop},
    {"Deauth Attack!", deauth_attack::enter,     deauth_attack::exit,    deauth_attack::loop},
    {"Beacon Spam!",   beacon_spam::enter,       beacon_spam::exit,      beacon_spam::loop},
    {"Probe Sniffer",  probe_sniffer::enter,     probe_sniffer::exit,    probe_sniffer::loop},
};
static const int NUM_ITEMS = sizeof(menuItems) / sizeof(menuItems[0]);

static Mode currentMode = Mode::MENU;
static int selIndex = 0;

static void renderMenu() {
    ui.clear();
    ui.header("Security Toolkit");

    const int visibleRows = 4;
    int windowStart = selIndex - 1;
    if (windowStart < 0) windowStart = 0;
    if (windowStart > NUM_ITEMS - visibleRows) windowStart = NUM_ITEMS - visibleRows;
    if (windowStart < 0) windowStart = 0;

    for (int row = 0; row < visibleRows; row++) {
        int i = windowStart + row;
        if (i >= NUM_ITEMS) break;
        ui.line(row, menuItems[i].name, i == selIndex);
    }
    ui.footer("SHORT=next LONG=open");
    ui.show();
}

void setup() {
    Serial.begin(115200);
    delay(200);

    storage::begin();
    serial_console::begin();

    navButton.begin();
    if (!ui.begin()) {
        Serial.println(F("[err] OLED not found - check SDA/SCL wiring in config.h"));
    }

    Serial.println(F("ESP32-S3 Security Toolkit ready."));
    renderMenu();
}

void loop() {
    serial_console::poll();
    ButtonEvent ev = navButton.poll();

    if (currentMode == Mode::MENU) {
        if (ev == ButtonEvent::SHORT) {
            selIndex = (selIndex + 1) % NUM_ITEMS;
        } else if (ev == ButtonEvent::LONG || ev == ButtonEvent::VLONG) {
            menuItems[selIndex].enterFn();
            currentMode = (Mode)(selIndex + 1); // menu items start right after MENU
        }
        renderMenu();
    } else {
        int idx = (int)currentMode - 1;
        if (ev == ButtonEvent::VLONG) {
            menuItems[idx].exitFn();
            currentMode = Mode::MENU;
            renderMenu();
        } else {
            menuItems[idx].loopFn(ev);
        }
    }

    yield();
}
