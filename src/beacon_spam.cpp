#include "beacon_spam.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include "config.h"

namespace beacon_spam {

static const char *ssidPool[] = {
    "Test_Network_1", "Test_Network_2", "Free_Public_WiFi",
    "Lab_Rogue_AP", "Marauder_Demo", "Spam_Test_SSID",
};
static const int POOL_LEN = sizeof(ssidPool) / sizeof(ssidPool[0]);

static int ssidIdx = 0;
static int currentChannel = 1;
static uint32_t lastSendMs = 0;
static uint32_t lastHopMs = 0;
static uint32_t sentCount = 0;
static bool running = true;
static uint16_t seqNum = 0;

static void randomMac(uint8_t *mac) {
    for (int i = 0; i < 6; i++) mac[i] = esp_random() & 0xFF;
    mac[0] = (mac[0] & 0xFC) | 0x02; // locally administered, unicast
}

static void sendBeacon(const char *ssid) {
    uint8_t mac[6];
    randomMac(mac);

    uint8_t ssidLen = strlen(ssid);
    uint8_t frame[128];
    int p = 0;

    frame[p++] = 0x80; frame[p++] = 0x00; // FC: mgmt / beacon
    frame[p++] = 0x00; frame[p++] = 0x00; // duration
    memset(&frame[p], 0xFF, 6); p += 6;   // addr1: broadcast
    memcpy(&frame[p], mac, 6); p += 6;    // addr2: spoofed source
    memcpy(&frame[p], mac, 6); p += 6;    // addr3: BSSID
    seqNum = (seqNum + 1) & 0x0FFF;
    frame[p++] = (seqNum << 4) & 0xFF;
    frame[p++] = (seqNum >> 4) & 0xFF;

    // Fixed parameters: timestamp(8, zero), beacon interval(2), capability(2)
    memset(&frame[p], 0x00, 8); p += 8;
    frame[p++] = 0x64; frame[p++] = 0x00;  // beacon interval ~100ms
    frame[p++] = 0x01; frame[p++] = 0x00;  // capability: ESS, open

    // Tagged: SSID
    frame[p++] = 0x00; frame[p++] = ssidLen;
    memcpy(&frame[p], ssid, ssidLen); p += ssidLen;

    // Tagged: supported rates
    const uint8_t rates[] = {0x82, 0x84, 0x8B, 0x96};
    frame[p++] = 0x01; frame[p++] = sizeof(rates);
    memcpy(&frame[p], rates, sizeof(rates)); p += sizeof(rates);

    // Tagged: DS parameter set (current channel)
    frame[p++] = 0x03; frame[p++] = 0x01;
    frame[p++] = (uint8_t)currentChannel;

    esp_wifi_80211_tx(WIFI_IF_AP, frame, p, false);
    sentCount++;
}

void enter() {
    WiFi.mode(WIFI_AP);
    currentChannel = 1;
    esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
    ssidIdx = 0;
    sentCount = 0;
    running = true;
    lastSendMs = 0;
    lastHopMs = millis();
}

void exit() {
    WiFi.mode(WIFI_MODE_NULL);
}

static void render() {
    ui.clear();
    ui.header(running ? "Beacon Spam" : "Beacon Spam [OFF]");
    ui.linef(0, "ssid: %s", ssidPool[ssidIdx]);
    ui.linef(1, "chan: %d", currentChannel);
    ui.linef(2, "sent: %lu", (unsigned long)sentCount);
    ui.footer("SHORT=pause/resume");
    ui.show();
}

void loop(ButtonEvent ev) {
    if (ev == ButtonEvent::SHORT) {
        running = !running;
    }

    uint32_t now = millis();
    if (running) {
        if (now - lastHopMs >= 400) {
            currentChannel = (currentChannel % 13) + 1;
            esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
            lastHopMs = now;
        }
        if (now - lastSendMs >= 60) {
            lastSendMs = now;
            sendBeacon(ssidPool[ssidIdx]);
            ssidIdx = (ssidIdx + 1) % POOL_LEN;
        }
    }

    render();
}

} // namespace beacon_spam
