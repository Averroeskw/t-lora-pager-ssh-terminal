/**
 * Settings UI for T-LoRa Pager Terminal
 * LVGL-based settings menu system
 */

#ifndef SETTINGS_UI_H
#define SETTINGS_UI_H

#include <lvgl.h>
#include "settings.h"

// Menu state
typedef enum {
    MENU_HIDDEN = 0,
    MENU_MAIN,
    MENU_DISPLAY,
    MENU_WIFI_LIST,
    MENU_WIFI_SCAN,
    MENU_WIFI_ADD,
    MENU_WIFI_EDIT,
    MENU_SERVER_LOCAL,
    MENU_SERVER_REMOTE,
    MENU_SYSTEM,
    MENU_ABOUT
} MenuState_t;

// Initialize settings UI
void settingsUIInit();

// Show/hide settings menu
void settingsUIShow();
void settingsUIHide();
bool settingsUIIsVisible();

// Handle rotary encoder input
void settingsUIHandleRotary(int direction);  // -1 = CCW, 0 = click, 1 = CW
void settingsUIHandleKey(char key);
void settingsUICancelInput();  // Cancel text input (WiFi add/edit)

// Get current menu state
MenuState_t settingsUIGetState();

// WiFi scanning
void settingsUIStartWiFiScan();
void settingsUIUpdateWiFiList();

// Apply theme to terminal
void applyThemeToTerminal();

#endif // SETTINGS_UI_H
