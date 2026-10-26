#include "storage.h"
#include <Preferences.h>
#include "config.h"

static Preferences prefs;

namespace storage {

void begin() {
    // Each call opens/closes prefs around itself (cheap, avoids holding
    // the NVS handle open across the whole sketch lifetime).
}

void setWifi(const String &ssid, const String &password) {
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putString("wifi_ssid", ssid);
    prefs.putString("wifi_pass", password);
    prefs.end();
}

bool getWifi(String &ssid, String &password) {
    prefs.begin(NVS_NAMESPACE, true);
    ssid = prefs.getString("wifi_ssid", "");
    password = prefs.getString("wifi_pass", "");
    prefs.end();
    return ssid.length() > 0;
}

bool hasWifi() {
    String s, p;
    return getWifi(s, p);
}

void clearWifi() {
    prefs.begin(NVS_NAMESPACE, false);
    prefs.remove("wifi_ssid");
    prefs.remove("wifi_pass");
    prefs.end();
}

int totpCount() {
    prefs.begin(NVS_NAMESPACE, true);
    int n = prefs.getInt("totp_n", 0);
    prefs.end();
    return n;
}

bool totpGet(int index, TotpAccount &out) {
    prefs.begin(NVS_NAMESPACE, true);
    int n = prefs.getInt("totp_n", 0);
    if (index < 0 || index >= n) {
        prefs.end();
        return false;
    }
    out.label = prefs.getString(("totp_l" + String(index)).c_str(), "");
    out.base32secret = prefs.getString(("totp_s" + String(index)).c_str(), "");
    prefs.end();
    return out.label.length() > 0;
}

bool totpAdd(const String &label, const String &base32secret) {
    prefs.begin(NVS_NAMESPACE, false);
    int n = prefs.getInt("totp_n", 0);

    // Overwrite if label already exists.
    for (int i = 0; i < n; i++) {
        String l = prefs.getString(("totp_l" + String(i)).c_str(), "");
        if (l == label) {
            prefs.putString(("totp_s" + String(i)).c_str(), base32secret);
            prefs.end();
            return true;
        }
    }

    if (n >= storage::TOTP_MAX_ACCOUNTS) {
        prefs.end();
        return false;
    }

    prefs.putString(("totp_l" + String(n)).c_str(), label);
    prefs.putString(("totp_s" + String(n)).c_str(), base32secret);
    prefs.putInt("totp_n", n + 1);
    prefs.end();
    return true;
}

bool totpRemove(const String &label) {
    prefs.begin(NVS_NAMESPACE, false);
    int n = prefs.getInt("totp_n", 0);
    int foundAt = -1;
    for (int i = 0; i < n; i++) {
        String l = prefs.getString(("totp_l" + String(i)).c_str(), "");
        if (l == label) {
            foundAt = i;
            break;
        }
    }
    if (foundAt < 0) {
        prefs.end();
        return false;
    }
    // Shift the rest down by one slot.
    for (int i = foundAt; i < n - 1; i++) {
        String l = prefs.getString(("totp_l" + String(i + 1)).c_str(), "");
        String s = prefs.getString(("totp_s" + String(i + 1)).c_str(), "");
        prefs.putString(("totp_l" + String(i)).c_str(), l);
        prefs.putString(("totp_s" + String(i)).c_str(), s);
    }
    prefs.remove(("totp_l" + String(n - 1)).c_str());
    prefs.remove(("totp_s" + String(n - 1)).c_str());
    prefs.putInt("totp_n", n - 1);
    prefs.end();
    return true;
}

} // namespace storage
