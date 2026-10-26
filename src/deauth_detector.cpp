#include "deauth_detector.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include "config.h"

namespace deauth_detector {

// 802.11 management frame subtypes we care about.
static const uint8_t SUBTYPE_DEAUTH   = 0x0C;
static const uint8_t SUBTYPE_DISASSOC = 0x0A;

struct Event {
    uint32_t tMs;
    bool isDeauth; // false = disassoc
};

static const int RING_LEN = 64;
static Event ring[RING_LEN];
static int ringHead = 0;
static int ringCount = 0;

static char lastSrcMac[18] = "--:--:--:--:--:--";
static uint8_t lastChannel = 0;
static int currentChannel = 1;
static uint32_t lastHopMs = 0;
static bool locked = false;
static uint32_t lastAlertFlashMs = 0;

static void macToStr(const uint8_t *mac, char *out) {
    snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void IRAM_ATTR onPacket(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT) return;
    auto *pkt = (wifi_promiscuous_pkt_t *)buf;
    const uint8_t *frame = pkt->payload;

    uint8_t frameType = (frame[0] >> 2) & 0x3;
    uint8_t subtype = (frame[0] >> 4) & 0xF;
    if (frameType != 0) return; // not management

    if (subtype != SUBTYPE_DEAUTH && subtype != SUBTYPE_DISASSOC) return;

    ring[ringHead].tMs = millis();
    ring[ringHead].isDeauth = (subtype == SUBTYPE_DEAUTH);
    ringHead = (ringHead + 1) % RING_LEN;
    if (ringCount < RING_LEN) ringCount++;

    // addr2 = transmitter address, offset 10 in the 802.11 header.
    macToStr(&frame[10], lastSrcMac);
    lastChannel = pkt->rx_ctrl.channel;
}

static int countRecent(uint32_t windowMs, bool deauthOnly) {
    uint32_t now = millis();
    int c = 0;
    for (int i = 0; i < ringCount; i++) {
        int idx = (ringHead - 1 - i + RING_LEN) % RING_LEN;
        if (now - ring[idx].tMs > windowMs) break;
        if (!deauthOnly || ring[idx].isDeauth) c++;
    }
    return c;
}

void enter() {
    WiFi.mode(WIFI_MODE_STA);
    WiFi.disconnect();
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_promiscuous(true);

    wifi_promiscuous_filter_t filter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT};
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(&onPacket);

    currentChannel = 1;
    esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
    lastHopMs = millis();
    ringHead = 0;
    ringCount = 0;
    locked = false;
    strcpy(lastSrcMac, "--:--:--:--:--:--");
}

void exit() {
    esp_wifi_set_promiscuous(false);
}

static void render(bool alert, int deauthRate, int disassocRate) {
    if (alert) {
        char l1[24], l2[24];
        snprintf(l1, sizeof(l1), "ch%d %d/s src", lastChannel, deauthRate + disassocRate);
        snprintf(l2, sizeof(l2), "%s", lastSrcMac);
        ui.alertBanner(l1, l2);
        return;
    }

    ui.clear();
    ui.header(locked ? "Deauth Det. [LOCK]" : "Deauth Detector");
    ui.linef(0, "scan ch: %d", locked ? lastChannel : currentChannel);
    ui.linef(1, "deauth/s:   %d", deauthRate);
    ui.linef(2, "disassoc/s: %d", disassocRate);
    ui.linef(3, "last: %s", lastSrcMac);
    ui.footer("SHORT=reset LONG=lock");
    ui.show();
}

void loop(ButtonEvent ev) {
    uint32_t now = millis();

    if (ev == ButtonEvent::SHORT) {
        ringHead = 0;
        ringCount = 0;
    } else if (ev == ButtonEvent::LONG) {
        locked = !locked;
        if (!locked) {
            currentChannel = 1;
            esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
            lastHopMs = now;
        }
    }

    if (!locked && now - lastHopMs >= DEAUTH_CHANNEL_HOP_MS) {
        currentChannel = (currentChannel % 13) + 1;
        esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
        lastHopMs = now;
    }

    int deauthRate = countRecent(DEAUTH_WINDOW_MS, true);
    int disassocRate = countRecent(DEAUTH_WINDOW_MS, false) - deauthRate;
    bool alert = (deauthRate + disassocRate) >= DEAUTH_ALERT_THRESHOLD;

    // Alert banner flashes on its own cadence regardless of main loop rate.
    if (alert || now - lastAlertFlashMs > 150) {
        lastAlertFlashMs = now;
        render(alert, deauthRate, disassocRate);
    }
}

} // namespace deauth_detector
