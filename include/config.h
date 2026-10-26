#pragma once
// ---------------------------------------------------------------------------
// Hardware pin map and global tunables.
// Board assumed: ESP32-S3 WROOM dev board + 0.96" SSD1306 OLED (I2C, 128x64).
// No extra hardware: navigation uses the board's built-in BOOT button
// (GPIO0 on every ESP32-S3 dev board), input (WiFi creds / TOTP secrets)
// uses the USB-serial console that's already there for flashing.
//
// If your board wires the OLED to different pins, change these two and
// nothing else needs to change.
// ---------------------------------------------------------------------------

// ---- OLED (I2C) -----------------------------------------------------------
#define OLED_SDA_PIN      8      // common default on ESP32-S3 dev boards
#define OLED_SCL_PIN      9
#define OLED_I2C_ADDR     0x3C
#define OLED_WIDTH        128
#define OLED_HEIGHT       64

// ---- Button (BOOT button, active LOW, no external wiring needed) ----------
#define BUTTON_PIN            0
#define BUTTON_DEBOUNCE_MS     30
#define BUTTON_LONG_PRESS_MS  600
#define BUTTON_VLONG_PRESS_MS 2500

// ---- Deauth detector --------------------------------------------------
#define DEAUTH_WINDOW_MS        1000   // sliding window for rate counting
#define DEAUTH_ALERT_THRESHOLD  5      // frames/sec that trigger an alert
#define DEAUTH_CHANNEL_HOP_MS   300    // time spent per channel while scanning

// ---- Evil twin detector -----------------------------------------------
#define EVILTWIN_MAX_SSIDS      24
#define EVILTWIN_MAX_BSSID_PER  4
#define EVILTWIN_RESCAN_MS       8000

// ---- Packet monitor -----------------------------------------------------
#define PKTMON_CHANNEL_HOP_MS   250
#define PKTMON_NUM_CHANNELS     13
#define PKTMON_HISTORY_LEN      20     // samples kept for anomaly baseline
#define PKTMON_ANOMALY_SIGMA    2.5f   // stddev multiplier for alarm

// ---- BLE scanner ---------------------------------------------------------
#define BLE_SCAN_WINDOW_SEC      5
#define BLE_SPAM_WINDOW_MS       10000
#define BLE_SPAM_UNIQUE_THRESHOLD 25   // distinct MACs/addr-changes in window

// ---- Port scanner (own network only!) ------------------------------------
#define PORTSCAN_HOST_TIMEOUT_MS 400
#define PORTSCAN_CONNECT_TIMEOUT_MS 200
#define PORTSCAN_MAX_HOSTS       254

// ---- TOTP -----------------------------------------------------------------
#define TOTP_STEP_SECONDS   30
#define TOTP_DIGITS         6
#define NTP_SERVER_1        "pool.ntp.org"
#define NTP_SERVER_2        "time.google.com"
#define NTP_GMT_OFFSET_SEC  0
#define NTP_DST_OFFSET_SEC  0

// NVS namespace used by Preferences across modules
#define NVS_NAMESPACE       "ckit"
