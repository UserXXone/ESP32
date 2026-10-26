#include "eviltwin_detector.h"
#include <WiFi.h>
#include "config.h"

namespace eviltwin_detector {

struct KnownAp {
    uint8_t bssid[6];
    wifi_auth_mode_t enc;
};

struct SsidEntry {
    String ssid;
    KnownAp aps[EVILTWIN_MAX_BSSID_PER];
    int apCount = 0;
    bool suspicious = false;
    char reason[20] = "";
};

static SsidEntry table[EVILTWIN_MAX_SSIDS];
static int tableCount = 0;
static bool scanning = false;
static uint32_t lastScanMs = 0;
static int scrollIndex = 0;
static int suspiciousCount = 0;

static bool bssidEq(const uint8_t *a, const uint8_t *b) {
    return memcmp(a, b, 6) == 0;
}

static int findEntry(const String &ssid) {
    for (int i = 0; i < tableCount; i++) {
        if (table[i].ssid == ssid) return i;
    }
    return -1;
}

static void resetBaseline() {
    tableCount = 0;
    suspiciousCount = 0;
    scrollIndex = 0;
}

static void startScan() {
    WiFi.scanDelete();
    WiFi.scanNetworks(/*async=*/true, /*show_hidden=*/true);
    scanning = true;
    lastScanMs = millis();
}

void enter() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(50);
    resetBaseline();
    startScan();
}

void exit() {
    WiFi.scanDelete();
    scanning = false;
}

static void ingestResults(int n) {
    // First pass: detect same-SSID / different-encryption within this one
    // scan directly (strongest, fastest signal).
    for (int i = 0; i < n; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) continue;

        int idx = findEntry(ssid);
        if (idx < 0) {
            if (tableCount >= EVILTWIN_MAX_SSIDS) continue;
            idx = tableCount++;
            table[idx] = SsidEntry();
            table[idx].ssid = ssid;
        }
        SsidEntry &e = table[idx];

        uint8_t *bssid = WiFi.BSSID(i);
        wifi_auth_mode_t enc = WiFi.encryptionType(i);

        // Already known transmitter for this SSID?
        int apIdx = -1;
        for (int k = 0; k < e.apCount; k++) {
            if (bssidEq(e.aps[k].bssid, bssid)) { apIdx = k; break; }
        }

        if (apIdx >= 0) {
            if (e.aps[apIdx].enc != enc) {
                e.suspicious = true;
                strcpy(e.reason, "enc changed");
                if (suspiciousCount == 0 || !e.suspicious) suspiciousCount++;
                e.aps[apIdx].enc = enc;
            }
            continue;
        }

        // New transmitter for an SSID we already track: compare encryption
        // against every BSSID already on file for this name.
        bool mismatchFound = false;
        for (int k = 0; k < e.apCount; k++) {
            if (e.aps[k].enc != enc) { mismatchFound = true; break; }
        }

        if (e.apCount < EVILTWIN_MAX_BSSID_PER) {
            memcpy(e.aps[e.apCount].bssid, bssid, 6);
            e.aps[e.apCount].enc = enc;
            e.apCount++;
        } else {
            mismatchFound = true; // too many distinct transmitters, flag too
            strcpy(e.reason, "many BSSIDs");
        }

        if (mismatchFound && e.apCount > 1) {
            if (!e.suspicious) suspiciousCount++;
            e.suspicious = true;
            if (e.reason[0] == '\0') strcpy(e.reason, "mixed encryption");
        }
    }
}

static void render() {
    ui.clear();
    char hdr[24];
    snprintf(hdr, sizeof(hdr), "Evil Twin (%d sus)", suspiciousCount);
    ui.header(hdr);

    if (scanning) {
        ui.line(0, "Scanning...");
        ui.footer("LONG=reset baseline");
        ui.show();
        return;
    }

    if (suspiciousCount == 0) {
        ui.line(0, "No anomalies seen");
        ui.linef(1, "%d SSIDs tracked", tableCount);
        ui.footer("LONG=reset baseline");
        ui.show();
        return;
    }

    int shown = 0;
    int visited = 0;
    for (int i = 0; i < tableCount && shown < 4; i++) {
        if (!table[i].suspicious) continue;
        if (visited++ < scrollIndex) continue;
        char buf[40];
        String name = table[i].ssid;
        if (name.length() > 12) name = name.substring(0, 12);
        snprintf(buf, sizeof(buf), "%-12s %s", name.c_str(), table[i].reason);
        ui.line(shown, buf);
        shown++;
    }
    ui.footer("SHORT=scroll LONG=reset");
    ui.show();
}

void loop(ButtonEvent ev) {
    if (scanning) {
        int n = WiFi.scanComplete();
        if (n >= 0) {
            ingestResults(n);
            scanning = false;
        } else if (n == WIFI_SCAN_FAILED) {
            scanning = false;
        }
    } else if (millis() - lastScanMs >= EVILTWIN_RESCAN_MS) {
        startScan();
    }

    if (ev == ButtonEvent::SHORT && suspiciousCount > 0) {
        scrollIndex = (scrollIndex + 1) % suspiciousCount;
    } else if (ev == ButtonEvent::LONG) {
        resetBaseline();
        startScan();
    }

    render();
}

} // namespace eviltwin_detector
