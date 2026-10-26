#include "probe_sniffer.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include "config.h"

namespace probe_sniffer {

static const uint8_t SUBTYPE_PROBE_REQ = 0x04;

struct ProbeEntry {
    char mac[18];
    char ssid[24];
    uint32_t count;
    uint32_t lastSeenMs;
    bool used;
};

static const int MAX_ENTRIES = 16;
static ProbeEntry entries[MAX_ENTRIES];
static int entryCount = 0;
static int scrollIndex = 0;
static int currentChannel = 1;
static uint32_t lastHopMs = 0;

static void macToStr(const uint8_t *mac, char *out) {
    snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void IRAM_ATTR onPacket(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT) return;
    auto *pkt = (wifi_promiscuous_pkt_t *)buf;
    const uint8_t *f = pkt->payload;

    uint8_t frameType = (f[0] >> 2) & 0x3;
    uint8_t subtype = (f[0] >> 4) & 0xF;
    if (frameType != 0 || subtype != SUBTYPE_PROBE_REQ) return;

    // Probe request body (tagged params) starts right after the 24-byte
    // header; first tag is normally the SSID element.
    const uint8_t *tag = &f[24];
    if (tag[0] != 0x00) return; // not an SSID tag
    uint8_t ssidLen = tag[1];
    if (ssidLen == 0 || ssidLen > 23) return; // skip wildcard/broadcast probes

    char mac[18];
    macToStr(&f[10], mac); // addr2 = transmitter

    char ssid[24];
    memcpy(ssid, &tag[2], ssidLen);
    ssid[ssidLen] = '\0';

    for (int i = 0; i < entryCount; i++) {
        if (entries[i].used && strcmp(entries[i].mac, mac) == 0 &&
            strcmp(entries[i].ssid, ssid) == 0) {
            entries[i].count++;
            entries[i].lastSeenMs = millis();
            return;
        }
    }

    int idx = entryCount < MAX_ENTRIES ? entryCount++ : 0; // else overwrite oldest slot 0
    if (entryCount >= MAX_ENTRIES) {
        uint32_t oldest = UINT32_MAX;
        for (int i = 0; i < MAX_ENTRIES; i++) {
            if (entries[i].lastSeenMs < oldest) { oldest = entries[i].lastSeenMs; idx = i; }
        }
    }
    strncpy(entries[idx].mac, mac, sizeof(entries[idx].mac));
    strncpy(entries[idx].ssid, ssid, sizeof(entries[idx].ssid));
    entries[idx].count = 1;
    entries[idx].lastSeenMs = millis();
    entries[idx].used = true;
}

void enter() {
    WiFi.mode(WIFI_MODE_STA);
    WiFi.disconnect();
    esp_wifi_set_promiscuous(true);
    wifi_promiscuous_filter_t filter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT};
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(&onPacket);
    entryCount = 0;
    scrollIndex = 0;
    currentChannel = 1;
    esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
    lastHopMs = millis();
}

void exit() {
    esp_wifi_set_promiscuous(false);
}

static void render() {
    ui.clear();
    char hdr[24];
    snprintf(hdr, sizeof(hdr), "Probe Sniff (%d)", entryCount);
    ui.header(hdr);

    if (entryCount == 0) {
        ui.line(0, "Listening...");
        ui.footer("LONG=clear");
        ui.show();
        return;
    }

    const int visibleRows = 4;
    for (int row = 0; row < visibleRows; row++) {
        int i = scrollIndex + row;
        if (i >= entryCount) break;
        char buf[40];
        snprintf(buf, sizeof(buf), "%.8s %-12s x%lu", entries[i].mac,
                 entries[i].ssid, (unsigned long)entries[i].count);
        ui.line(row, buf);
    }
    ui.footer("SHORT=scroll LONG=clear");
    ui.show();
}

void loop(ButtonEvent ev) {
    uint32_t now = millis();
    if (now - lastHopMs >= 300) {
        currentChannel = (currentChannel % 13) + 1;
        esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
        lastHopMs = now;
    }

    if (ev == ButtonEvent::SHORT && entryCount > 0) {
        scrollIndex++;
        if (scrollIndex >= entryCount) scrollIndex = 0;
    } else if (ev == ButtonEvent::LONG) {
        entryCount = 0;
        scrollIndex = 0;
    }

    render();
}

} // namespace probe_sniffer
