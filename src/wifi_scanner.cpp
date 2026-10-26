#include "wifi_scanner.h"
#include <WiFi.h>

namespace wifi_scanner {

static bool scanning = false;
static int resultCount = -2; // -2 = not started, -1 = running, >=0 = done
static int scrollIndex = 0;

static const char *encName(wifi_auth_mode_t mode, bool *weak) {
    *weak = false;
    switch (mode) {
        case WIFI_AUTH_OPEN:            *weak = true;  return "OPEN";
        case WIFI_AUTH_WEP:             *weak = true;  return "WEP";
        case WIFI_AUTH_WPA_PSK:         *weak = true;  return "WPA";
        case WIFI_AUTH_WPA2_PSK:        return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-E";
        case WIFI_AUTH_WPA3_PSK:        return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/3";
        case WIFI_AUTH_WAPI_PSK:        return "WAPI";
        default:                        return "?";
    }
}

static void startScan() {
    ui.clear();
    ui.header("WiFi Scan");
    ui.line(0, "Scanning...");
    ui.show();
    WiFi.scanDelete();
    WiFi.scanNetworks(/*async=*/true, /*show_hidden=*/true);
    scanning = true;
    resultCount = -1;
    scrollIndex = 0;
}

void enter() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(50);
    startScan();
}

void exit() {
    WiFi.scanDelete();
    scanning = false;
}

static void render() {
    ui.clear();
    char hdr[24];
    snprintf(hdr, sizeof(hdr), "WiFi Scan (%d)", resultCount < 0 ? 0 : resultCount);
    ui.header(hdr);

    if (resultCount <= 0) {
        ui.line(0, resultCount == 0 ? "No networks found" : "Scanning...");
        ui.footer("LONG=rescan");
        ui.show();
        return;
    }

    // 4 visible rows, scrollable with SHORT press.
    const int visibleRows = 4;
    for (int row = 0; row < visibleRows; row++) {
        int i = scrollIndex + row;
        if (i >= resultCount) break;
        bool weak;
        const char *enc = encName(WiFi.encryptionType(i), &weak);
        char buf[40];
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) ssid = "<hidden>";
        if (ssid.length() > 11) ssid = ssid.substring(0, 11);
        snprintf(buf, sizeof(buf), "%s%-11s c%2d %4d %s",
                 weak ? "!" : " ", ssid.c_str(), (int)WiFi.channel(i), (int)WiFi.RSSI(i), enc);
        ui.line(row, buf);
    }
    ui.footer("SHORT=scroll LONG=rescan");
    ui.show();
}

void loop(ButtonEvent ev) {
    if (scanning) {
        int n = WiFi.scanComplete();
        if (n >= 0) {
            scanning = false;
            resultCount = n;
        } else if (n == WIFI_SCAN_FAILED) {
            scanning = false;
            resultCount = 0;
        }
    }

    if (ev == ButtonEvent::SHORT && resultCount > 0) {
        scrollIndex++;
        if (scrollIndex >= resultCount) scrollIndex = 0;
    } else if (ev == ButtonEvent::LONG) {
        startScan();
    }

    render();
}

} // namespace wifi_scanner
