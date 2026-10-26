#include "totp_generator.h"
#include <WiFi.h>
#include <time.h>
#include <mbedtls/md.h>
#include "config.h"
#include "storage.h"

namespace totp_generator {

enum class State { NO_ACCOUNTS, NO_WIFI_CREDS, CONNECTING, SYNCING, READY, ERROR };
static State state = State::NO_ACCOUNTS;

static TotpAccount accounts[storage::TOTP_MAX_ACCOUNTS];
static int accountCount = 0;
static int currentIdx = 0;

static bool timeSynced = false; // persists across re-entering this mode
static uint32_t connectStartMs = 0;

// ---- base32 decode (RFC 4648, no padding required) -------------------
static int base32Decode(const String &in, uint8_t *out, int outMax) {
    static const char *ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    int buffer = 0, bitsLeft = 0, count = 0;
    for (size_t i = 0; i < in.length(); i++) {
        char c = toupper(in[i]);
        if (c == '=') break;
        const char *p = strchr(ALPHABET, c);
        if (!p) continue; // skip separators/invalid chars
        int val = p - ALPHABET;
        buffer = (buffer << 5) | val;
        bitsLeft += 5;
        if (bitsLeft >= 8) {
            if (count >= outMax) break;
            out[count++] = (buffer >> (bitsLeft - 8)) & 0xFF;
            bitsLeft -= 8;
        }
    }
    return count;
}

static uint32_t computeCode(const String &base32secret, time_t epoch) {
    uint8_t key[32];
    int keyLen = base32Decode(base32secret, key, sizeof(key));
    if (keyLen <= 0) return 0;

    uint64_t counter = (uint64_t)(epoch / TOTP_STEP_SECONDS);
    uint8_t msg[8];
    for (int i = 7; i >= 0; i--) {
        msg[i] = counter & 0xFF;
        counter >>= 8;
    }

    uint8_t digest[20];
    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, info, 1 /* hmac */);
    mbedtls_md_hmac_starts(&ctx, key, keyLen);
    mbedtls_md_hmac_update(&ctx, msg, sizeof(msg));
    mbedtls_md_hmac_finish(&ctx, digest);
    mbedtls_md_free(&ctx);

    int offset = digest[19] & 0x0F;
    uint32_t binCode = ((digest[offset] & 0x7F) << 24) |
                        ((digest[offset + 1] & 0xFF) << 16) |
                        ((digest[offset + 2] & 0xFF) << 8) |
                        (digest[offset + 3] & 0xFF);

    uint32_t mod = 1;
    for (int i = 0; i < TOTP_DIGITS; i++) mod *= 10;
    return binCode % mod;
}

static void loadAccounts() {
    accountCount = 0;
    int n = storage::totpCount();
    for (int i = 0; i < n && i < storage::TOTP_MAX_ACCOUNTS; i++) {
        if (storage::totpGet(i, accounts[accountCount])) accountCount++;
    }
    if (currentIdx >= accountCount) currentIdx = 0;
}

static void beginSync() {
    String ssid, pass;
    if (!storage::getWifi(ssid, pass)) {
        state = State::NO_WIFI_CREDS;
        return;
    }
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    connectStartMs = millis();
    state = State::CONNECTING;
}

void enter() {
    loadAccounts();
    if (accountCount == 0) {
        state = State::NO_ACCOUNTS;
        return;
    }
    if (timeSynced) {
        state = State::READY;
        return;
    }
    beginSync();
}

void exit() {
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect();
    }
}

static void render() {
    ui.clear();
    ui.header("TOTP Generator");

    switch (state) {
        case State::NO_ACCOUNTS:
            ui.line(0, "No accounts stored.");
            ui.line(1, "Serial console:");
            ui.line(2, "totp add <lbl> <secret>");
            break;

        case State::NO_WIFI_CREDS:
            ui.line(0, "Need WiFi for clock.");
            ui.line(1, "wifi set <ssid> <pw>");
            ui.footer("then LONG=retry");
            break;

        case State::CONNECTING:
            ui.linef(0, "Connecting%s", (millis() / 400) % 2 ? "..." : "");
            if (millis() - connectStartMs > 15000) state = State::ERROR;
            break;

        case State::SYNCING:
            ui.line(0, "Syncing time (NTP)...");
            break;

        case State::ERROR:
            ui.line(0, "WiFi/NTP failed");
            ui.footer("LONG=retry");
            break;

        case State::READY: {
            if (accountCount == 0) {
                ui.line(0, "No accounts stored.");
                break;
            }
            time_t now;
            time(&now);
            uint32_t code = computeCode(accounts[currentIdx].base32secret, now);
            uint32_t remaining = TOTP_STEP_SECONDS - (now % TOTP_STEP_SECONDS);

            char codeBuf[8];
            char fmt[8];
            snprintf(fmt, sizeof(fmt), "%%0%dlu", TOTP_DIGITS);
            snprintf(codeBuf, sizeof(codeBuf), fmt, (unsigned long)code);

            ui.linef(0, "%s", accounts[currentIdx].label.c_str());
            ui.raw().setTextSize(2);
            ui.raw().setCursor(8, 26);
            // Insert a visual gap in the middle, e.g. "123 456".
            String disp = codeBuf;
            if (disp.length() == 6) disp = disp.substring(0, 3) + " " + disp.substring(3);
            ui.raw().print(disp);
            ui.raw().setTextSize(1);

            // Countdown bar.
            int w = (int)(((float)remaining / TOTP_STEP_SECONDS) * OLED_WIDTH);
            ui.raw().drawRect(0, 48, OLED_WIDTH, 6, SSD1306_WHITE);
            ui.raw().fillRect(1, 49, max(0, w - 2), 4, SSD1306_WHITE);
            break;
        }
    }

    if (state == State::READY && accountCount > 1) {
        ui.footer("SHORT=next acct");
    }
    ui.show();
}

void loop(ButtonEvent ev) {
    if (ev == ButtonEvent::SHORT && state == State::READY && accountCount > 1) {
        currentIdx = (currentIdx + 1) % accountCount;
    } else if (ev == ButtonEvent::LONG) {
        if (state == State::NO_WIFI_CREDS || state == State::ERROR) {
            beginSync();
        } else if (state == State::READY) {
            timeSynced = false;
            beginSync();
        }
    }

    if (state == State::CONNECTING && WiFi.status() == WL_CONNECTED) {
        configTime(NTP_GMT_OFFSET_SEC, NTP_DST_OFFSET_SEC, NTP_SERVER_1, NTP_SERVER_2);
        state = State::SYNCING;
        connectStartMs = millis();
    } else if (state == State::SYNCING) {
        time_t now;
        time(&now);
        if (now > 1700000000) { // sane epoch => synced
            timeSynced = true;
            state = State::READY;
            WiFi.disconnect();
        } else if (millis() - connectStartMs > 10000) {
            state = State::ERROR;
        }
    }

    render();
}

} // namespace totp_generator
