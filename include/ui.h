#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include "config.h"

// Thin wrapper around Adafruit_SSD1306 with a few layout helpers every
// module reuses: a title bar, scrollable text lists and a full-screen
// flashing alert banner.
class Ui {
public:
    bool begin();

    void clear();
    void show();

    // Title bar at the top + thin separator line.
    void header(const char *title);

    // Prints a line of body text at the given row (0-based, rows below the
    // header). Truncates to fit the screen width.
    void line(uint8_t row, const char *text, bool inverted = false);
    void linef(uint8_t row, const char *fmt, ...);

    // Small status glyph bottom-right (e.g. battery not applicable, we use
    // it for "scanning..." spinner / mode hint).
    void footer(const char *text);

    // Full-screen inverted flashing banner used by detectors when they
    // trigger (call repeatedly from loop(); it alternates on its own).
    void alertBanner(const char *line1, const char *line2);

    Adafruit_SSD1306 &raw() { return display; }

private:
    Adafruit_SSD1306 display{OLED_WIDTH, OLED_HEIGHT, &Wire, -1};
    bool ok = false;
};

extern Ui ui;

// ---------------------------------------------------------------------------
// Single-button navigation: BOOT button, active LOW. Classified once, on
// release, by how long it was held:
//   SHORT  (<600ms)        -> NEXT     (cycle menu / scroll a list)
//   LONG   (600ms-2.5s)    -> SELECT   (enter mode / confirm / mode action)
//   VLONG  (>=2.5s)        -> HOME     (always: back to the main menu)
// Keeping HOME on a distinct, longer hold means a mode is free to use LONG
// for its own action (e.g. "rescan") without it ever being confused with
// "leave this mode".
// ---------------------------------------------------------------------------
enum class ButtonEvent { NONE, SHORT, LONG, VLONG };

class Button {
public:
    void begin();
    ButtonEvent poll();

private:
    bool lastReading = true; // idle = HIGH (pull-up)
    bool stableState = true;
    uint32_t lastChangeMs = 0;
    uint32_t pressStartMs = 0;
};

extern Button navButton;
