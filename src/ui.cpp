#include "ui.h"
#include <stdarg.h>
#include <stdio.h>

Ui ui;
Button navButton;

bool Ui::begin() {
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
    ok = display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR);
    if (!ok) {
        return false;
    }
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.cp437(true);
    clear();
    show();
    return true;
}

void Ui::clear() {
    if (!ok) return;
    display.clearDisplay();
}

void Ui::show() {
    if (!ok) return;
    display.display();
}

void Ui::header(const char *title) {
    if (!ok) return;
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(title);
    display.drawFastHLine(0, 10, OLED_WIDTH, SSD1306_WHITE);
}

void Ui::line(uint8_t row, const char *text, bool inverted) {
    if (!ok) return;
    int16_t y = 13 + row * 10;
    if (y > OLED_HEIGHT - 8) return;
    if (inverted) {
        display.fillRect(0, y - 1, OLED_WIDTH, 9, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
    }
    display.setCursor(0, y);
    display.print(text);
    if (inverted) {
        display.setTextColor(SSD1306_WHITE);
    }
}

void Ui::linef(uint8_t row, const char *fmt, ...) {
    char buf[40];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    line(row, buf);
}

void Ui::footer(const char *text) {
    if (!ok) return;
    display.setCursor(0, OLED_HEIGHT - 8);
    display.print(text);
}

void Ui::alertBanner(const char *line1, const char *line2) {
    if (!ok) return;
    static bool flash = false;
    flash = !flash;
    display.clearDisplay();
    if (flash) {
        display.fillRect(0, 0, OLED_WIDTH, OLED_HEIGHT, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
    } else {
        display.setTextColor(SSD1306_WHITE);
    }
    display.setTextSize(2);
    display.setCursor(4, 12);
    display.print("! ALERT !");
    display.setTextSize(1);
    display.setCursor(2, 36);
    display.print(line1);
    display.setCursor(2, 48);
    display.print(line2);
    display.setTextColor(SSD1306_WHITE);
    display.display();
}

// ---------------------------------------------------------------------------

void Button::begin() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    lastReading = digitalRead(BUTTON_PIN);
    stableState = lastReading;
    lastChangeMs = millis();
}

ButtonEvent Button::poll() {
    bool reading = digitalRead(BUTTON_PIN);
    uint32_t now = millis();

    if (reading != lastReading) {
        lastChangeMs = now;
        lastReading = reading;
    }

    ButtonEvent ev = ButtonEvent::NONE;

    if ((now - lastChangeMs) > BUTTON_DEBOUNCE_MS && reading != stableState) {
        stableState = reading;
        if (stableState == LOW) {
            // button just pressed
            pressStartMs = now;
        } else {
            // button just released: classify by total hold time
            uint32_t held = now - pressStartMs;
            if (held >= BUTTON_VLONG_PRESS_MS) {
                ev = ButtonEvent::VLONG;
            } else if (held >= BUTTON_LONG_PRESS_MS) {
                ev = ButtonEvent::LONG;
            } else {
                ev = ButtonEvent::SHORT;
            }
        }
    }

    return ev;
}
