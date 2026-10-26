#include "ble_scanner.h"
#include <NimBLEDevice.h>
#include "config.h"

namespace ble_scanner {

struct BleDevice {
    String addr;
    int rssi = -127;
    String name;
    bool appleTracker = false;
    uint32_t lastSeenMs = 0;
    bool used = false;
};

static const int MAX_DEVICES = 20;
static BleDevice devices[MAX_DEVICES];
static int deviceCount = 0;

// Ring of "first time we saw this address" timestamps, for spam detection.
static const int SPAM_RING_LEN = 48;
static uint32_t spamRing[SPAM_RING_LEN];
static int spamHead = 0;
static int spamFilled = 0;

static int scrollIndex = 0;
static NimBLEScan *pScan = nullptr;

static bool looksLikeAppleFindMy(NimBLEAdvertisedDevice *dev) {
    if (!dev->haveManufacturerData()) return false;
    std::string md = dev->getManufacturerData();
    if (md.length() < 3) return false;
    uint16_t companyId = (uint8_t)md[0] | ((uint8_t)md[1] << 8);
    if (companyId != 0x004C) return false; // Apple Inc.
    uint8_t type = (uint8_t)md[2];
    // 0x12 = "Find My" / offline finding broadcast type.
    return type == 0x12;
}

class ScanCb : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice *dev) override {
        String addr = dev->getAddress().toString().c_str();
        uint32_t now = millis();

        int idx = -1;
        for (int i = 0; i < deviceCount; i++) {
            if (devices[i].used && devices[i].addr == addr) { idx = i; break; }
        }

        bool firstSeen = (idx < 0);

        if (idx < 0) {
            if (deviceCount < MAX_DEVICES) {
                idx = deviceCount++;
            } else {
                // Evict the stalest entry (simple LRU).
                uint32_t oldest = UINT32_MAX;
                for (int i = 0; i < MAX_DEVICES; i++) {
                    if (devices[i].lastSeenMs < oldest) {
                        oldest = devices[i].lastSeenMs;
                        idx = i;
                    }
                }
            }
            devices[idx] = BleDevice();
            devices[idx].addr = addr;
            devices[idx].used = true;
        }

        devices[idx].rssi = dev->getRSSI();
        devices[idx].lastSeenMs = now;
        if (dev->haveName()) {
            devices[idx].name = dev->getName().c_str();
        }
        devices[idx].appleTracker = looksLikeAppleFindMy(dev);

        if (firstSeen) {
            spamRing[spamHead] = now;
            spamHead = (spamHead + 1) % SPAM_RING_LEN;
            if (spamFilled < SPAM_RING_LEN) spamFilled++;
        }
    }
};

static ScanCb scanCb;

static int spamRate() {
    uint32_t now = millis();
    int c = 0;
    for (int i = 0; i < spamFilled; i++) {
        int idx = (spamHead - 1 - i + SPAM_RING_LEN) % SPAM_RING_LEN;
        if (now - spamRing[idx] > BLE_SPAM_WINDOW_MS) break;
        c++;
    }
    return c;
}

static void resetTable() {
    deviceCount = 0;
    spamHead = 0;
    spamFilled = 0;
    scrollIndex = 0;
    for (auto &d : devices) d = BleDevice();
}

void enter() {
    NimBLEDevice::init("");
    pScan = NimBLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(&scanCb, /*wantDuplicates=*/true);
    pScan->setActiveScan(true);
    pScan->setInterval(45);
    pScan->setWindow(15);
    resetTable();
    pScan->start(0 /* forever, until stop() */, nullptr, false);
}

void exit() {
    if (pScan) pScan->stop();
    NimBLEDevice::deinit(true);
}

// Simple descending-RSSI ordering for display, built fresh each render
// (table is tiny, so a selection sort each frame is cheap enough).
static void sortedIndices(int *order) {
    for (int i = 0; i < deviceCount; i++) order[i] = i;
    for (int i = 0; i < deviceCount; i++) {
        int best = i;
        for (int j = i + 1; j < deviceCount; j++) {
            if (devices[order[j]].rssi > devices[order[best]].rssi) best = j;
        }
        int t = order[i]; order[i] = order[best]; order[best] = t;
    }
}

static void render(int rate, bool alert) {
    if (alert) {
        char l2[24];
        snprintf(l2, sizeof(l2), "%d new addr/10s", rate);
        ui.alertBanner("BLE spam/flood", l2);
        return;
    }

    ui.clear();
    char hdr[24];
    snprintf(hdr, sizeof(hdr), "BLE Scan (%d)", deviceCount);
    ui.header(hdr);

    if (deviceCount == 0) {
        ui.line(0, "Scanning...");
        ui.footer("LONG=clear");
        ui.show();
        return;
    }

    int order[MAX_DEVICES];
    sortedIndices(order);

    const int visibleRows = 4;
    for (int row = 0; row < visibleRows; row++) {
        int pos = scrollIndex + row;
        if (pos >= deviceCount) break;
        BleDevice &d = devices[order[pos]];
        String label = d.name.length() ? d.name : d.addr.substring(0, 11);
        if (label.length() > 11) label = label.substring(0, 11);
        char buf[40];
        snprintf(buf, sizeof(buf), "%s%-11s %4d", d.appleTracker ? "*" : " ",
                 label.c_str(), d.rssi);
        ui.line(row, buf);
    }
    ui.footer("SHORT=scroll LONG=clear");
    ui.show();
}

void loop(ButtonEvent ev) {
    if (ev == ButtonEvent::SHORT && deviceCount > 0) {
        scrollIndex++;
        if (scrollIndex >= deviceCount) scrollIndex = 0;
    } else if (ev == ButtonEvent::LONG) {
        resetTable();
    }

    int rate = spamRate();
    bool alert = rate >= BLE_SPAM_UNIQUE_THRESHOLD;
    render(rate, alert);
}

} // namespace ble_scanner
