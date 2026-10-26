#include "packet_monitor.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <math.h>
#include "config.h"

namespace packet_monitor {

static volatile uint32_t channelCounts[PKTMON_NUM_CHANNELS + 1]; // 1-indexed
static uint32_t cycleHistory[PKTMON_HISTORY_LEN];
static int historyLen = 0;
static int historyPos = 0;

static int currentChannel = 1;
static uint32_t lastHopMs = 0;
static bool anomalyActive = false;
static int anomalyChannel = 0;
static bool locked = false;

static void IRAM_ATTR onPacket(void *buf, wifi_promiscuous_pkt_type_t type) {
    auto *pkt = (wifi_promiscuous_pkt_t *)buf;
    uint8_t ch = pkt->rx_ctrl.channel;
    if (ch >= 1 && ch <= PKTMON_NUM_CHANNELS) {
        channelCounts[ch] = channelCounts[ch] + 1; // avoid ++ on volatile (deprecated)
    }
}

void enter() {
    WiFi.mode(WIFI_MODE_STA);
    WiFi.disconnect();
    esp_wifi_set_promiscuous(true);
    wifi_promiscuous_filter_t filter = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA};
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(&onPacket);

    memset((void *)channelCounts, 0, sizeof(channelCounts));
    historyLen = 0;
    historyPos = 0;
    currentChannel = 1;
    esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
    lastHopMs = millis();
    anomalyActive = false;
    locked = false;
}

void exit() {
    esp_wifi_set_promiscuous(false);
}

static void recordCycleTotal() {
    uint32_t total = 0;
    uint32_t busiest = 0;
    int busiestCh = 1;
    for (int ch = 1; ch <= PKTMON_NUM_CHANNELS; ch++) {
        total += channelCounts[ch];
        if (channelCounts[ch] > busiest) {
            busiest = channelCounts[ch];
            busiestCh = ch;
        }
    }

    if (historyLen >= 5) {
        float mean = 0;
        for (int i = 0; i < historyLen; i++) mean += cycleHistory[i];
        mean /= historyLen;
        float var = 0;
        for (int i = 0; i < historyLen; i++) {
            float d = cycleHistory[i] - mean;
            var += d * d;
        }
        var /= historyLen;
        float stddev = sqrtf(var);
        if (stddev > 0.001f && (total - mean) > PKTMON_ANOMALY_SIGMA * stddev) {
            anomalyActive = true;
            anomalyChannel = busiestCh;
        } else {
            anomalyActive = false;
        }
    }

    cycleHistory[historyPos] = total;
    historyPos = (historyPos + 1) % PKTMON_HISTORY_LEN;
    if (historyLen < PKTMON_HISTORY_LEN) historyLen++;

    memset((void *)channelCounts, 0, sizeof(channelCounts));
}

static void render(uint32_t total) {
    if (anomalyActive) {
        char l2[24];
        snprintf(l2, sizeof(l2), "busiest ch %d", anomalyChannel);
        ui.alertBanner("Traffic spike", l2);
        return;
    }

    ui.clear();
    ui.header(locked ? "Pkt Monitor [LOCK]" : "Packet Monitor");

    // Bar graph: 13 channels across the width, rows 13..53 (40px tall).
    uint32_t maxCount = 1;
    for (int ch = 1; ch <= PKTMON_NUM_CHANNELS; ch++) {
        if (channelCounts[ch] > maxCount) maxCount = channelCounts[ch];
    }
    const int barAreaTop = 13;
    const int barAreaH = 36;
    const int barW = OLED_WIDTH / PKTMON_NUM_CHANNELS;
    for (int ch = 1; ch <= PKTMON_NUM_CHANNELS; ch++) {
        int h = (int)((uint64_t)channelCounts[ch] * barAreaH / maxCount);
        int x = (ch - 1) * barW;
        int y = barAreaTop + (barAreaH - h);
        ui.raw().fillRect(x, y, barW - 1, h, SSD1306_WHITE);
    }
    ui.raw().drawFastHLine(0, barAreaTop + barAreaH, OLED_WIDTH, SSD1306_WHITE);

    char buf[32];
    snprintf(buf, sizeof(buf), "ch%d total:%lu", currentChannel, (unsigned long)total);
    ui.line(4, buf);
    ui.footer("SHORT=lock LONG=reset");
    ui.show();
}

void loop(ButtonEvent ev) {
    uint32_t now = millis();

    if (ev == ButtonEvent::SHORT) {
        locked = !locked;
    } else if (ev == ButtonEvent::LONG) {
        historyLen = 0;
        historyPos = 0;
        anomalyActive = false;
    }

    if (!locked && now - lastHopMs >= PKTMON_CHANNEL_HOP_MS) {
        currentChannel++;
        if (currentChannel > PKTMON_NUM_CHANNELS) {
            recordCycleTotal();
            currentChannel = 1;
        }
        esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
        lastHopMs = now;
    }

    uint32_t total = 0;
    for (int ch = 1; ch <= PKTMON_NUM_CHANNELS; ch++) total += channelCounts[ch];
    render(total);
}

} // namespace packet_monitor
