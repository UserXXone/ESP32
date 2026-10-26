#include "serial_console.h"
#include <Arduino.h>
#include "storage.h"

namespace serial_console {

static String inbuf;

static void printHelp() {
    Serial.println(F(
        "Commands:\n"
        "  help\n"
        "  wifi set <ssid> <password>\n"
        "  wifi show\n"
        "  wifi clear\n"
        "  totp add <label> <base32secret>\n"
        "  totp del <label>\n"
        "  totp list"));
}

// Splits on the first N-1 spaces, leaving the remainder (e.g. a password
// that might itself contain spaces) intact in the last token.
static int splitArgs(const String &line, String *out, int maxParts) {
    int count = 0;
    int start = 0;
    while (count < maxParts - 1) {
        int sp = line.indexOf(' ', start);
        if (sp < 0) break;
        out[count++] = line.substring(start, sp);
        start = sp + 1;
    }
    if (start <= (int)line.length()) {
        out[count++] = line.substring(start);
    }
    return count;
}

static void handleLine(String line) {
    line.trim();
    if (line.length() == 0) return;

    String parts[4];
    int n = splitArgs(line, parts, 4);
    if (n == 0) return;

    if (parts[0] == "help") {
        printHelp();
    } else if (parts[0] == "wifi" && n >= 2) {
        if (parts[1] == "set" && n >= 4) {
            storage::setWifi(parts[2], parts[3]);
            Serial.println(F("[ok] wifi credentials stored"));
        } else if (parts[1] == "show") {
            String ssid, pass;
            if (storage::getWifi(ssid, pass)) {
                Serial.printf("ssid=%s password=<hidden, %u chars>\n", ssid.c_str(), pass.length());
            } else {
                Serial.println(F("no wifi credentials stored"));
            }
        } else if (parts[1] == "clear") {
            storage::clearWifi();
            Serial.println(F("[ok] wifi credentials cleared"));
        } else {
            Serial.println(F("usage: wifi set <ssid> <password> | wifi show | wifi clear"));
        }
    } else if (parts[0] == "totp" && n >= 2) {
        if (parts[1] == "add" && n >= 4) {
            String secret = parts[3];
            secret.toUpperCase();
            secret.replace(" ", "");
            if (storage::totpAdd(parts[2], secret)) {
                Serial.println(F("[ok] totp account stored"));
            } else {
                Serial.println(F("[err] account list full (max 8)"));
            }
        } else if (parts[1] == "del" && n >= 3) {
            if (storage::totpRemove(parts[2])) {
                Serial.println(F("[ok] removed"));
            } else {
                Serial.println(F("[err] label not found"));
            }
        } else if (parts[1] == "list") {
            int c = storage::totpCount();
            if (c == 0) {
                Serial.println(F("no totp accounts stored"));
            }
            for (int i = 0; i < c; i++) {
                TotpAccount acc;
                if (storage::totpGet(i, acc)) {
                    Serial.printf("  %d: %s\n", i, acc.label.c_str());
                }
            }
        } else {
            Serial.println(F("usage: totp add <label> <secret> | totp del <label> | totp list"));
        }
    } else {
        Serial.println(F("unknown command, type 'help'"));
    }
}

void begin() {
    inbuf.reserve(96);
    Serial.println(F("\n== ESP32-S3 Security Toolkit console (type 'help') =="));
}

void poll() {
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            if (inbuf.length() > 0) {
                handleLine(inbuf);
                inbuf = "";
            }
        } else {
            if (inbuf.length() < 160) {
                inbuf += c;
            }
        }
    }
}

} // namespace serial_console
