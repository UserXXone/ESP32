#include "port_scanner.h"
#include <WiFi.h>
#include <ESP32Ping.h>
#include "config.h"
#include "storage.h"

namespace port_scanner {

enum class State { NO_CREDS, CONNECTING, IDLE, PINGING, PORTSCAN, DONE, ERROR };
static State state = State::NO_CREDS;

static const uint16_t candidatePorts[] = {
    21, 22, 23, 25, 53, 80, 110, 139, 143, 443,
    445, 993, 995, 1723, 3306, 3389, 5900, 8080, 8443};
static const int NUM_PORTS = sizeof(candidatePorts) / sizeof(candidatePorts[0]);

struct HostResult {
    uint8_t lastOctet;
    uint32_t openMask; // bit i -> candidatePorts[i] open
};
static const int MAX_LIVE_HOSTS = 32;
static HostResult liveHosts[MAX_LIVE_HOSTS];
static int liveHostCount = 0;

static uint32_t networkBase = 0; // first host address, as u32
static uint32_t hostSpan = 0;    // number of host addresses to try (capped)
static uint32_t sweepIndex = 0;  // current offset into the sweep
static uint32_t ownU32 = 0;

static int scanHostIdx = 0;
static int scanPortIdx = 0;

static uint32_t connectStartMs = 0;
static int scrollIndex = 0;

static uint32_t ipToU32(IPAddress ip) {
    return ((uint32_t)ip[0] << 24) | ((uint32_t)ip[1] << 16) | ((uint32_t)ip[2] << 8) | ip[3];
}
static IPAddress u32ToIp(uint32_t v) {
    return IPAddress((v >> 24) & 0xFF, (v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
}

static void beginConnect() {
    String ssid, pass;
    if (!storage::getWifi(ssid, pass)) {
        state = State::NO_CREDS;
        return;
    }
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    connectStartMs = millis();
    state = State::CONNECTING;
}

void enter() {
    liveHostCount = 0;
    scrollIndex = 0;
    beginConnect();
}

void exit() {
    WiFi.disconnect();
}

static void startSweep() {
    IPAddress ip = WiFi.localIP();
    IPAddress mask = WiFi.subnetMask();
    uint32_t ipU = ipToU32(ip);
    uint32_t maskU = ipToU32(mask);
    uint32_t network = ipU & maskU;
    uint32_t broadcast = network | (~maskU);

    networkBase = network + 1;
    uint32_t totalHosts = (broadcast - 1) - networkBase + 1;
    if (totalHosts > PORTSCAN_MAX_HOSTS) totalHosts = PORTSCAN_MAX_HOSTS;
    hostSpan = totalHosts;
    sweepIndex = 0;
    ownU32 = ipU;
    liveHostCount = 0;
    state = State::PINGING;
}

static void pingStep() {
    if (sweepIndex >= hostSpan) {
        scanHostIdx = 0;
        scanPortIdx = 0;
        state = liveHostCount > 0 ? State::PORTSCAN : State::DONE;
        return;
    }
    uint32_t candidate = networkBase + sweepIndex;
    sweepIndex++;
    if (candidate == ownU32) return; // skip ourselves

    IPAddress target = u32ToIp(candidate);
    if (Ping.ping(target, 1) && liveHostCount < MAX_LIVE_HOSTS) {
        liveHosts[liveHostCount].lastOctet = candidate & 0xFF;
        liveHosts[liveHostCount].openMask = 0;
        liveHostCount++;
    }
}

static void portStep() {
    if (scanHostIdx >= liveHostCount) {
        state = State::DONE;
        return;
    }
    // Reconstruct the full IP from the stored last octet + current /24 base.
    IPAddress target = u32ToIp((networkBase & 0xFFFFFF00) | liveHosts[scanHostIdx].lastOctet);

    WiFiClient client;
    client.setTimeout(PORTSCAN_CONNECT_TIMEOUT_MS);
    uint16_t port = candidatePorts[scanPortIdx];
    if (client.connect(target, port, PORTSCAN_CONNECT_TIMEOUT_MS)) {
        liveHosts[scanHostIdx].openMask |= (1UL << scanPortIdx);
        client.stop();
    }

    scanPortIdx++;
    if (scanPortIdx >= NUM_PORTS) {
        scanPortIdx = 0;
        scanHostIdx++;
    }
}

static void render() {
    ui.clear();

    switch (state) {
        case State::NO_CREDS:
            ui.header("Port Scanner");
            ui.line(0, "No WiFi creds set.");
            ui.line(1, "Use serial console:");
            ui.line(2, "wifi set <ssid> <pw>");
            ui.footer("then LONG=retry");
            break;

        case State::CONNECTING:
            ui.header("Port Scanner");
            ui.linef(0, "Connecting%s", (millis() / 400) % 2 ? "..." : "");
            if (millis() - connectStartMs > 15000) {
                state = State::ERROR;
            }
            break;

        case State::ERROR:
            ui.header("Port Scanner");
            ui.line(0, "WiFi connect failed");
            ui.footer("LONG=retry");
            break;

        case State::IDLE: {
            ui.header("Port Scanner");
            ui.linef(0, "IP: %s", WiFi.localIP().toString().c_str());
            ui.linef(1, "GW: %s", WiFi.gatewayIP().toString().c_str());
            ui.linef(2, "Range: up to %lu hosts", (unsigned long)PORTSCAN_MAX_HOSTS);
            ui.footer("LONG=start scan");
            break;
        }

        case State::PINGING: {
            ui.header("Port Scanner");
            ui.line(0, "Finding live hosts...");
            ui.linef(1, "%lu/%lu checked", (unsigned long)sweepIndex, (unsigned long)hostSpan);
            ui.linef(2, "%d found", liveHostCount);
            ui.footer("LONG=abort");
            break;
        }

        case State::PORTSCAN: {
            ui.header("Port Scanner");
            ui.linef(0, "Probing host %d/%d", scanHostIdx + 1, liveHostCount);
            if (scanHostIdx < liveHostCount) {
                ui.linef(1, ".%d  port %d", liveHosts[scanHostIdx].lastOctet,
                         candidatePorts[scanPortIdx]);
            }
            ui.footer("LONG=abort");
            break;
        }

        case State::DONE: {
            char hdr[24];
            snprintf(hdr, sizeof(hdr), "Results (%d hosts)", liveHostCount);
            ui.header(hdr);
            if (liveHostCount == 0) {
                ui.line(0, "No live hosts found");
            } else {
                int idx = scrollIndex % liveHostCount;
                ui.linef(0, "Host .%d", liveHosts[idx].lastOctet);
                int row = 1;
                bool any = false;
                for (int p = 0; p < NUM_PORTS && row < 4; p++) {
                    if (liveHosts[idx].openMask & (1UL << p)) {
                        ui.linef(row++, " open: %d", candidatePorts[p]);
                        any = true;
                    }
                }
                if (!any) ui.line(1, " (no ports open)");
            }
            ui.footer("SHORT=next host LONG=rescan");
            break;
        }
    }

    ui.show();
}

void loop(ButtonEvent ev) {
    if (ev == ButtonEvent::LONG) {
        switch (state) {
            case State::NO_CREDS:
            case State::ERROR:
                beginConnect();
                break;
            case State::IDLE:
                startSweep();
                break;
            case State::PINGING:
            case State::PORTSCAN:
                state = State::IDLE; // abort
                break;
            case State::DONE:
                startSweep();
                break;
            default:
                break;
        }
    } else if (ev == ButtonEvent::SHORT && state == State::DONE && liveHostCount > 0) {
        scrollIndex = (scrollIndex + 1) % liveHostCount;
    }

    if (state == State::CONNECTING && WiFi.status() == WL_CONNECTED) {
        state = State::IDLE;
    } else if (state == State::PINGING) {
        pingStep();
    } else if (state == State::PORTSCAN) {
        portStep();
    }

    render();
}

} // namespace port_scanner
