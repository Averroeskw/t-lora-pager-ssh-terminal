/**
 * Settings UI Implementation
 */

#include "settings_ui.h"
#include "settings.h"
#include <WiFi.h>
#include <LilyGoLib.h>

// UI state
static MenuState_t currentMenu = MENU_HIDDEN;
static int selectedIndex = 0;
static int scrollOffset = 0;
static int maxVisibleItems = 5;  // Items visible without scrolling

// Haptic feedback functions (use DRV2605 motor driver)
void playHapticClick() {
    if (!settings.hapticEnabled) return;
    instance.drv.setWaveform(0, 1);   // Strong click
    instance.drv.setWaveform(1, 0);   // End
    instance.drv.run();
}

void playHapticTick() {
    if (!settings.hapticEnabled) return;
    instance.drv.setWaveform(0, 10);  // Short tick
    instance.drv.setWaveform(1, 0);
    instance.drv.run();
}

void playHapticBump() {
    if (!settings.hapticEnabled) return;
    instance.drv.setWaveform(0, 47);  // Bump
    instance.drv.setWaveform(1, 0);
    instance.drv.run();
}

void playHapticDouble() {
    if (!settings.hapticEnabled) return;
    instance.drv.setWaveform(0, 1);   // Click
    instance.drv.setWaveform(1, 1);   // Click again
    instance.drv.setWaveform(2, 0);
    instance.drv.run();
}

// LVGL objects
static lv_obj_t *settingsScreen = NULL;
static lv_obj_t *menuContainer = NULL;
static lv_obj_t *titleLabel = NULL;
static lv_obj_t *menuList = NULL;
static lv_obj_t *statusLabel = NULL;
static lv_obj_t *inputTA = NULL;      // Text input area
static lv_obj_t *keyboard = NULL;     // On-screen keyboard

// WiFi scan results
#define MAX_SCAN_RESULTS 20
static String scanSSIDs[MAX_SCAN_RESULTS];
static int32_t scanRSSIs[MAX_SCAN_RESULTS];
static int scanCount = 0;
static bool scanning = false;

// Edit state
static int editingNetworkIndex = -1;
static int editingField = 0;  // 0=SSID, 1=Password
static char editBuffer[MAX_PASS_LEN];

// Forward declarations
static void createMainMenu();
static void createDisplayMenu();
static void createWiFiListMenu();
static void createWiFiScanMenu();
static void createWiFiAddMenu(const char* ssid);
static void createWiFiEditMenu(int index);
static void createServerMenu(bool isRemote);
static void createSystemMenu();
static void createAboutMenu();
static void updateMenuHighlight();
static void goBack();

// External reference to terminal text area for theme application
extern lv_obj_t *terminalTA;
extern lv_obj_t *statusBar;
extern lv_obj_t *statusLabel;

void settingsUIInit() {
    // Settings screen will be created on demand
}

void settingsUIShow() {
    if (settingsScreen == NULL) {
        // Create settings screen
        settingsScreen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(settingsScreen, lv_color_hex(0x000000), 0);
    }

    currentMenu = MENU_MAIN;
    selectedIndex = 0;
    scrollOffset = 0;
    createMainMenu();
    lv_scr_load(settingsScreen);
}

void settingsUIHide() {
    currentMenu = MENU_HIDDEN;
    // Return to terminal screen
    extern lv_obj_t *terminalTA;
    if (terminalTA) {
        lv_scr_load(lv_obj_get_parent(terminalTA));
    }
}

bool settingsUIIsVisible() {
    return currentMenu != MENU_HIDDEN;
}

MenuState_t settingsUIGetState() {
    return currentMenu;
}

static void clearMenu() {
    if (menuContainer) {
        lv_obj_del(menuContainer);
        menuContainer = NULL;
    }
    menuList = NULL;
    titleLabel = NULL;
    statusLabel = NULL;
    inputTA = NULL;
}

static lv_obj_t* createMenuContainer(const char* title, int totalItems = 0) {
    clearMenu();

    menuContainer = lv_obj_create(settingsScreen);
    lv_obj_set_size(menuContainer, 480, 222);
    lv_obj_set_pos(menuContainer, 0, 0);
    lv_obj_set_style_bg_color(menuContainer, lv_color_hex(0x111111), 0);
    lv_obj_set_style_border_width(menuContainer, 0, 0);
    lv_obj_set_style_pad_all(menuContainer, 0, 0);
    lv_obj_remove_flag(menuContainer, LV_OBJ_FLAG_SCROLLABLE);

    // Title bar
    lv_obj_t *titleBar = lv_obj_create(menuContainer);
    lv_obj_set_size(titleBar, 480, 24);
    lv_obj_set_pos(titleBar, 0, 0);
    lv_obj_set_style_bg_color(titleBar, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(titleBar, 0, 0);
    lv_obj_remove_flag(titleBar, LV_OBJ_FLAG_SCROLLABLE);

    // Title with optional scroll indicator
    char titleBuf[64];
    if (totalItems > maxVisibleItems) {
        snprintf(titleBuf, sizeof(titleBuf), "%s [%d/%d]", title, selectedIndex + 1, totalItems);
    } else {
        strncpy(titleBuf, title, sizeof(titleBuf) - 1);
    }
    titleLabel = lv_label_create(titleBar);
    lv_label_set_text(titleLabel, titleBuf);
    lv_obj_set_style_text_color(titleLabel, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_14, 0);
    lv_obj_align(titleLabel, LV_ALIGN_LEFT_MID, 10, 0);

    // Back hint
    lv_obj_t *backLabel = lv_label_create(titleBar);
    lv_label_set_text(backLabel, "hold=back");
    lv_obj_set_style_text_color(backLabel, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_font(backLabel, &lv_font_montserrat_10, 0);
    lv_obj_align(backLabel, LV_ALIGN_RIGHT_MID, -10, 0);

    // Status bar at bottom
    lv_obj_t *statusBar = lv_obj_create(menuContainer);
    lv_obj_set_size(statusBar, 480, 20);
    lv_obj_set_pos(statusBar, 0, 202);
    lv_obj_set_style_bg_color(statusBar, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(statusBar, 0, 0);
    lv_obj_remove_flag(statusBar, LV_OBJ_FLAG_SCROLLABLE);

    statusLabel = lv_label_create(statusBar);
    lv_label_set_text(statusLabel, "Rotate knob to scroll, click to select");
    lv_obj_set_style_text_color(statusLabel, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(statusLabel, &lv_font_montserrat_10, 0);
    lv_obj_align(statusLabel, LV_ALIGN_LEFT_MID, 5, 0);

    return menuContainer;
}

static lv_obj_t* addMenuItem(lv_obj_t *list, const char* text, const char* value, int index) {
    // Skip items outside visible range
    if (index < scrollOffset || index >= scrollOffset + maxVisibleItems) {
        return NULL;
    }

    lv_obj_t *item = lv_obj_create(list);
    lv_obj_set_size(item, 460, 28);
    lv_obj_set_style_bg_color(item, lv_color_hex(index == selectedIndex ? 0x004400 : 0x1A1A1A), 0);
    lv_obj_set_style_border_width(item, 0, 0);
    lv_obj_set_style_pad_all(item, 4, 0);
    lv_obj_remove_flag(item, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label = lv_label_create(item);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 5, 0);

    if (value && strlen(value) > 0) {
        lv_obj_t *valLabel = lv_label_create(item);
        lv_label_set_text(valLabel, value);
        lv_obj_set_style_text_color(valLabel, lv_color_hex(0x00FF00), 0);
        lv_obj_set_style_text_font(valLabel, &lv_font_montserrat_12, 0);
        lv_obj_align(valLabel, LV_ALIGN_RIGHT_MID, -5, 0);
    }

    return item;
}

static lv_obj_t* createMenuList() {
    menuList = lv_obj_create(menuContainer);
    lv_obj_set_size(menuList, 480, 175);
    lv_obj_set_pos(menuList, 0, 26);
    lv_obj_set_style_bg_color(menuList, lv_color_hex(0x111111), 0);
    lv_obj_set_style_border_width(menuList, 0, 0);
    lv_obj_set_style_pad_all(menuList, 8, 0);
    lv_obj_set_flex_flow(menuList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(menuList, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(menuList, 4, 0);
    return menuList;
}

static void createMainMenu() {
    const int totalItems = 7;
    createMenuContainer("SETTINGS", totalItems);
    createMenuList();

    addMenuItem(menuList, "[X] Close Settings", "", 0);

    char buf[32];
    snprintf(buf, sizeof(buf), "%d%%", (settings.brightness * 100) / 255);
    addMenuItem(menuList, "Display Settings", buf, 1);

    snprintf(buf, sizeof(buf), "%d saved", settings.wifiNetworkCount);
    addMenuItem(menuList, "WiFi Networks", buf, 2);

    addMenuItem(menuList, "Local Server", settings.localServer.enabled ? "ON" : "OFF", 3);
    addMenuItem(menuList, "Remote Server", settings.remoteServer.enabled ? "ON" : "OFF", 4);
    addMenuItem(menuList, "System", "", 5);
    addMenuItem(menuList, "About", "", 6);
}

static void createDisplayMenu() {
    const int totalItems = 3;
    createMenuContainer("DISPLAY", totalItems);
    createMenuList();

    addMenuItem(menuList, "[< Back]", "", 0);

    char buf[32];
    snprintf(buf, sizeof(buf), "%d%%", (settings.brightness * 100) / 255);
    addMenuItem(menuList, "Brightness", buf, 1);

    addMenuItem(menuList, "Theme", themeColors[settings.theme].name, 2);

    if (statusLabel) {
        lv_label_set_text(statusLabel, "Rotate to adjust");
    }
}

static void createWiFiListMenu() {
    const int totalItems = settings.wifiNetworkCount + 3;  // Back + Scan + networks + Add
    createMenuContainer("WIFI", totalItems);
    createMenuList();

    addMenuItem(menuList, "[< Back]", "", 0);
    addMenuItem(menuList, "[~] Scan Networks", "", 1);

    for (int i = 0; i < settings.wifiNetworkCount && i < MAX_WIFI_NETWORKS; i++) {
        char label[48];
        char status[16];
        if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == settings.wifiNetworks[i].ssid) {
            strcpy(status, "* Connected");
        } else {
            strcpy(status, settings.wifiNetworks[i].enabled ? "" : "Disabled");
        }
        addMenuItem(menuList, settings.wifiNetworks[i].ssid, status, i + 2);
    }

    addMenuItem(menuList, "[+] Add Network", "", settings.wifiNetworkCount + 2);

    if (statusLabel) {
        lv_label_set_text(statusLabel, "Click network to connect/delete");
    }
}

static void createWiFiScanMenu() {
    const int totalItems = scanning ? 1 : scanCount + 1;
    createMenuContainer(scanning ? "SCANNING..." : "SELECT NETWORK", totalItems);
    createMenuList();

    addMenuItem(menuList, "[< Back]", "", 0);

    if (scanning) {
        lv_obj_t *label = lv_label_create(menuList);
        lv_label_set_text(label, "Scanning...");
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFF00), 0);
    } else if (scanCount == 0) {
        lv_obj_t *label = lv_label_create(menuList);
        lv_label_set_text(label, "No networks found");
        lv_obj_set_style_text_color(label, lv_color_hex(0xFF0000), 0);
    } else {
        for (int i = 0; i < scanCount && i < MAX_SCAN_RESULTS; i++) {
            char rssiStr[16];
            snprintf(rssiStr, sizeof(rssiStr), "%ddBm", scanRSSIs[i]);
            addMenuItem(menuList, scanSSIDs[i].c_str(), rssiStr, i + 1);
        }
    }

    if (statusLabel) {
        lv_label_set_text(statusLabel, "Select to add");
    }
}

// WiFi input state
static char wifiSSID[MAX_SSID_LEN] = "";
static char wifiPass[MAX_PASS_LEN] = "";

static void createWiFiAddMenu(const char* ssid) {
    clearMenu();

    menuContainer = lv_obj_create(settingsScreen);
    lv_obj_set_size(menuContainer, 480, 222);
    lv_obj_set_pos(menuContainer, 0, 0);
    lv_obj_set_style_bg_color(menuContainer, lv_color_hex(0x111111), 0);
    lv_obj_set_style_border_width(menuContainer, 0, 0);
    lv_obj_set_style_pad_all(menuContainer, 0, 0);
    lv_obj_remove_flag(menuContainer, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t *titleBar = lv_obj_create(menuContainer);
    lv_obj_set_size(titleBar, 480, 24);
    lv_obj_set_pos(titleBar, 0, 0);
    lv_obj_set_style_bg_color(titleBar, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(titleBar, 0, 0);
    lv_obj_remove_flag(titleBar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(titleBar);
    lv_label_set_text(title, "ADD WIFI NETWORK");
    lv_obj_set_style_text_color(title, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 10, 0);

    // If SSID provided (from scan), use it
    if (ssid && strlen(ssid) > 0) {
        strncpy(wifiSSID, ssid, MAX_SSID_LEN - 1);
        editingField = 1;  // Go to password
    } else {
        wifiSSID[0] = '\0';
        editingField = 0;  // Start with SSID
    }
    wifiPass[0] = '\0';

    // SSID label and display
    lv_obj_t *ssidLbl = lv_label_create(menuContainer);
    lv_label_set_text(ssidLbl, "SSID:");
    lv_obj_set_style_text_color(ssidLbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_pos(ssidLbl, 20, 40);

    lv_obj_t *ssidVal = lv_label_create(menuContainer);
    lv_label_set_text(ssidVal, strlen(wifiSSID) > 0 ? wifiSSID : "(scanning or type below)");
    lv_obj_set_style_text_color(ssidVal, lv_color_hex(0x00FF00), 0);
    lv_obj_set_pos(ssidVal, 100, 40);

    // Password label
    lv_obj_t *passLbl = lv_label_create(menuContainer);
    lv_label_set_text(passLbl, "Password:");
    lv_obj_set_style_text_color(passLbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_pos(passLbl, 20, 70);

    // Current field indicator
    lv_obj_t *fieldLbl = lv_label_create(menuContainer);
    if (editingField == 0) {
        lv_label_set_text(fieldLbl, "> Enter SSID:");
    } else {
        lv_label_set_text(fieldLbl, "> Enter Password:");
    }
    lv_obj_set_style_text_color(fieldLbl, lv_color_hex(0xFFFF00), 0);
    lv_obj_set_pos(fieldLbl, 20, 100);

    // Text input area
    inputTA = lv_textarea_create(menuContainer);
    lv_obj_set_size(inputTA, 350, 35);
    lv_obj_set_pos(inputTA, 20, 125);
    lv_textarea_set_one_line(inputTA, true);
    lv_textarea_set_placeholder_text(inputTA, editingField == 0 ? "Type network name..." : "Type password...");
    lv_textarea_set_password_mode(inputTA, editingField == 1);
    lv_obj_set_style_bg_color(inputTA, lv_color_hex(0x222222), 0);
    lv_obj_set_style_text_color(inputTA, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_border_color(inputTA, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_border_width(inputTA, 1, 0);

    // Instructions
    lv_obj_t *helpLbl = lv_label_create(menuContainer);
    lv_label_set_text(helpLbl, "Type text, ENTER=next/save, long-press knob=cancel");
    lv_obj_set_style_text_color(helpLbl, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(helpLbl, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(helpLbl, 20, 170);

    // Status bar
    lv_obj_t *statBar = lv_obj_create(menuContainer);
    lv_obj_set_size(statBar, 480, 20);
    lv_obj_set_pos(statBar, 0, 202);
    lv_obj_set_style_bg_color(statBar, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(statBar, 0, 0);
    lv_obj_remove_flag(statBar, LV_OBJ_FLAG_SCROLLABLE);

    statusLabel = lv_label_create(statBar);
    lv_label_set_text(statusLabel, editingField == 0 ? "Step 1/2: Enter network name" : "Step 2/2: Enter password");
    lv_obj_set_style_text_color(statusLabel, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(statusLabel, &lv_font_montserrat_10, 0);
    lv_obj_align(statusLabel, LV_ALIGN_LEFT_MID, 5, 0);
}

// Server editing state
static bool editingRemoteServer = false;
static int serverEditField = -1;  // Which field being edited

static void createServerMenu(bool isRemote) {
    editingRemoteServer = isRemote;
    ServerConfig_t *server = isRemote ? &settings.remoteServer : &settings.localServer;
    const int totalItems = 8;
    createMenuContainer(isRemote ? "REMOTE" : "LOCAL", totalItems);
    createMenuList();

    addMenuItem(menuList, "[< Back]", "", 0);
    addMenuItem(menuList, "Enabled", server->enabled ? "ON" : "OFF", 1);
    addMenuItem(menuList, "Host", server->host, 2);

    char portStr[8];
    snprintf(portStr, sizeof(portStr), "%d", server->port);
    addMenuItem(menuList, "Port", portStr, 3);

    addMenuItem(menuList, "Path", server->path, 4);
    addMenuItem(menuList, "SSL", server->useSSL ? "ON" : "OFF", 5);
    addMenuItem(menuList, "[Test Connection]", "", 6);
    addMenuItem(menuList, "[Connect Now]", "", 7);

    if (statusLabel) {
        lv_label_set_text(statusLabel, "Click to toggle/edit");
    }
}

static void createSystemMenu() {
    const int totalItems = 9;
    createMenuContainer("SYSTEM", totalItems);
    createMenuList();

    addMenuItem(menuList, "[< Back]", "", 0);

    // Sound settings
    addMenuItem(menuList, "Sound", settings.soundEnabled ? "ON" : "OFF", 1);
    char volStr[8];
    snprintf(volStr, sizeof(volStr), "%d%%", settings.volume);
    addMenuItem(menuList, "Volume", volStr, 2);

    // Haptic settings
    addMenuItem(menuList, "Haptic", settings.hapticEnabled ? "ON" : "OFF", 3);
    char hapStr[8];
    snprintf(hapStr, sizeof(hapStr), "%d%%", settings.hapticIntensity);
    addMenuItem(menuList, "Haptic Intensity", hapStr, 4);

    // Connection settings
    addMenuItem(menuList, "Auto-connect", settings.wifiAutoConnect ? "ON" : "OFF", 5);
    addMenuItem(menuList, "Prefer Remote", settings.preferRemote ? "ON" : "OFF", 6);

    // Actions
    addMenuItem(menuList, "[Reset Settings]", "", 7);
    addMenuItem(menuList, "[Restart]", "", 8);

    if (statusLabel) {
        lv_label_set_text(statusLabel, "Rotate to adjust values");
    }
}

static void createAboutMenu() {
    createMenuContainer("ABOUT", 1);
    createMenuList();

    addMenuItem(menuList, "[< Back]", "", 0);

    lv_obj_t *info = lv_label_create(menuList);
    lv_label_set_text(info,
        "\nT-LoRa Pager Terminal v1.0\n"
        "WebSocket client for ttyd\n\n"
        "ESP32-S3 @ 240MHz\n"
        "Display: 480x222 ST7796\n"
        "Radio: SX1262 LoRa"
    );
    lv_obj_set_style_text_color(info, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(info, &lv_font_montserrat_12, 0);

    if (statusLabel) {
        lv_label_set_text(statusLabel, "");
    }
}

static void goBack() {
    playHapticClick();
    switch (currentMenu) {
        case MENU_MAIN:
            settingsUIHide();
            break;
        case MENU_DISPLAY:
        case MENU_WIFI_LIST:
        case MENU_SERVER_LOCAL:
        case MENU_SERVER_REMOTE:
        case MENU_SYSTEM:
        case MENU_ABOUT:
            currentMenu = MENU_MAIN;
            selectedIndex = 0;
            scrollOffset = 0;
            createMainMenu();
            break;
        case MENU_WIFI_SCAN:
        case MENU_WIFI_ADD:
        case MENU_WIFI_EDIT:
            currentMenu = MENU_WIFI_LIST;
            selectedIndex = 0;
            scrollOffset = 0;
            createWiFiListMenu();
            break;
        default:
            settingsUIHide();
            break;
    }
}

static void handleMainMenuSelect() {
    playHapticClick();
    scrollOffset = 0;
    switch (selectedIndex) {
        case 0:  // Close
            settingsUIHide();
            break;
        case 1:  // Display
            currentMenu = MENU_DISPLAY;
            selectedIndex = 0;
            createDisplayMenu();
            break;
        case 2:  // WiFi
            currentMenu = MENU_WIFI_LIST;
            selectedIndex = 0;
            createWiFiListMenu();
            break;
        case 3:  // Local Server
            currentMenu = MENU_SERVER_LOCAL;
            selectedIndex = 0;
            createServerMenu(false);
            break;
        case 4:  // Remote Server
            currentMenu = MENU_SERVER_REMOTE;
            selectedIndex = 0;
            createServerMenu(true);
            break;
        case 5:  // System
            currentMenu = MENU_SYSTEM;
            selectedIndex = 0;
            createSystemMenu();
            break;
        case 6:  // About
            currentMenu = MENU_ABOUT;
            selectedIndex = 0;
            createAboutMenu();
            break;
    }
}

static void handleDisplaySelect() {
    playHapticClick();
    if (selectedIndex == 0) {
        goBack();
    }
    // Other items adjusted with rotary
}

static void handleDisplayAdjust(int direction) {
    if (selectedIndex == 1) {
        // Brightness
        int newBrightness = settings.brightness + (direction * 25);
        if (newBrightness < 10) newBrightness = 10;
        if (newBrightness > 255) newBrightness = 255;
        settings.brightness = newBrightness;
        instance.setBrightness(settings.brightness);
        settingsSave();
        createDisplayMenu();
    } else if (selectedIndex == 2) {
        // Theme
        int newTheme = settings.theme + direction;
        if (newTheme < 0) newTheme = THEME_COUNT - 1;
        if (newTheme >= THEME_COUNT) newTheme = 0;
        settings.theme = (Theme_t)newTheme;
        settingsSave();
        applyThemeToTerminal();
        createDisplayMenu();
    }
}

static void handleSystemAdjust(int direction) {
    switch (selectedIndex) {
        case 1:  // Sound toggle
            settings.soundEnabled = !settings.soundEnabled;
            settingsSave();
            createSystemMenu();
            break;
        case 2:  // Volume
            {
                int newVol = settings.volume + (direction * 10);
                if (newVol < 0) newVol = 0;
                if (newVol > 100) newVol = 100;
                settings.volume = newVol;
                settingsSave();
                createSystemMenu();
            }
            break;
        case 3:  // Haptic toggle
            settings.hapticEnabled = !settings.hapticEnabled;
            settingsSave();
            playHapticClick();
            createSystemMenu();
            break;
        case 4:  // Haptic Intensity
            {
                int newHap = settings.hapticIntensity + (direction * 10);
                if (newHap < 0) newHap = 0;
                if (newHap > 100) newHap = 100;
                settings.hapticIntensity = newHap;
                settingsSave();
                playHapticTick();
                createSystemMenu();
            }
            break;
        case 5:  // Auto-connect toggle
            settings.wifiAutoConnect = !settings.wifiAutoConnect;
            settingsSave();
            createSystemMenu();
            break;
        case 6:  // Prefer Remote toggle
            settings.preferRemote = !settings.preferRemote;
            settingsSave();
            createSystemMenu();
            break;
    }
}

static void handleSystemSelect() {
    playHapticClick();
    switch (selectedIndex) {
        case 0:  // Back
            goBack();
            break;
        case 1:  // Toggle sound
        case 3:  // Toggle haptic
        case 5:  // Toggle auto-connect
        case 6:  // Toggle prefer remote
            handleSystemAdjust(1);  // Toggle
            break;
        case 7:  // Reset all settings
            playHapticDouble();
            settingsReset();
            settingsSave();
            createSystemMenu();
            break;
        case 8:  // Restart device
            playHapticDouble();
            delay(200);
            ESP.restart();
            break;
    }
}

void settingsUIStartWiFiScan() {
    scanning = true;
    scanCount = 0;
    createWiFiScanMenu();

    // Start async scan
    WiFi.scanNetworks(true);
}

void settingsUIUpdateWiFiList() {
    int n = WiFi.scanComplete();
    if (n >= 0) {
        scanning = false;
        scanCount = min(n, MAX_SCAN_RESULTS);
        for (int i = 0; i < scanCount; i++) {
            scanSSIDs[i] = WiFi.SSID(i);
            scanRSSIs[i] = WiFi.RSSI(i);
        }
        WiFi.scanDelete();
        createWiFiScanMenu();
    }
}

// Delete WiFi network by index
static void deleteWiFiNetwork(int index) {
    if (index < 0 || index >= settings.wifiNetworkCount) return;

    // Shift networks down
    for (int i = index; i < settings.wifiNetworkCount - 1; i++) {
        memcpy(&settings.wifiNetworks[i], &settings.wifiNetworks[i + 1], sizeof(WiFiNetwork_t));
    }
    settings.wifiNetworkCount--;
    settingsSave();
    Serial.printf("Deleted network at index %d, count now %d\n", index, settings.wifiNetworkCount);
}

static void handleWiFiListSelect() {
    playHapticClick();
    if (selectedIndex == 0) {
        // Back
        goBack();
    } else if (selectedIndex == 1) {
        // Scan
        currentMenu = MENU_WIFI_SCAN;
        selectedIndex = 0;
        settingsUIStartWiFiScan();
    } else if (selectedIndex <= settings.wifiNetworkCount + 1) {
        // Selected a saved network - show options (connect or delete)
        int netIndex = selectedIndex - 2;
        if (netIndex >= 0 && netIndex < settings.wifiNetworkCount) {
            // For now, delete on select (could add submenu later)
            // Toggle enabled/disabled instead of delete
            settings.wifiNetworks[netIndex].enabled = !settings.wifiNetworks[netIndex].enabled;
            settingsSave();
            createWiFiListMenu();
        }
    } else {
        // Add manual
        currentMenu = MENU_WIFI_ADD;
        selectedIndex = 0;
        editingNetworkIndex = -1;
        createWiFiAddMenu(NULL);
    }
}

static void handleWiFiScanSelect() {
    playHapticClick();
    if (selectedIndex == 0) {
        // Back
        goBack();
    } else if (selectedIndex <= scanCount) {
        // Add selected network
        currentMenu = MENU_WIFI_ADD;
        createWiFiAddMenu(scanSSIDs[selectedIndex - 1].c_str());
    }
}

// Forward declaration for reconnect
extern void connectToServer();

static void handleServerSelect() {
    playHapticClick();
    ServerConfig_t *server = editingRemoteServer ? &settings.remoteServer : &settings.localServer;

    switch (selectedIndex) {
        case 0:  // Back
            goBack();
            break;
        case 1:  // Toggle enabled
            server->enabled = !server->enabled;
            settingsSave();
            createServerMenu(editingRemoteServer);
            break;
        case 5:  // Toggle SSL
            server->useSSL = !server->useSSL;
            settingsSave();
            createServerMenu(editingRemoteServer);
            break;
        case 6:  // Test connection
            // TODO: Implement test
            break;
        case 7:  // Connect now
            settingsUIHide();
            connectToServer();
            break;
    }
}

static void saveWiFiNetwork(const char* ssid, const char* password) {
    Serial.printf("saveWiFiNetwork: SSID='%s' Pass='%s'\n", ssid, password);
    if (settings.wifiNetworkCount < MAX_WIFI_NETWORKS) {
        int idx = settings.wifiNetworkCount;
        strncpy(settings.wifiNetworks[idx].ssid, ssid, MAX_SSID_LEN - 1);
        strncpy(settings.wifiNetworks[idx].password, password, MAX_PASS_LEN - 1);
        settings.wifiNetworks[idx].enabled = true;
        settings.wifiNetworkCount++;
        Serial.printf("Network saved at index %d, count now %d\n", idx, settings.wifiNetworkCount);
        settingsSave();
    } else {
        Serial.println("ERROR: Max WiFi networks reached!");
    }
}

// Cancel WiFi input (called on long-press)
void settingsUICancelInput() {
    Serial.println("Long-press: Cancelling input");
    playHapticBump();
    wifiSSID[0] = '\0';
    wifiPass[0] = '\0';
    editingField = 0;
    currentMenu = MENU_WIFI_LIST;
    selectedIndex = 0;
    scrollOffset = 0;
    createWiFiListMenu();
}

void settingsUIHandleKey(char key) {
    // Handle text input if in add/edit mode
    if (inputTA && (currentMenu == MENU_WIFI_ADD || currentMenu == MENU_WIFI_EDIT)) {
        Serial.printf("WiFi Input: key=%d('%c') field=%d\n", key, key >= 32 ? key : '?', editingField);
        if (key >= 32 && key < 127) {
            lv_textarea_add_char(inputTA, key);
        } else if (key == '\b' || key == 127) {
            lv_textarea_delete_char(inputTA);
        } else if (key == '\n' || key == '\r') {
            // Get entered text
            const char* text = lv_textarea_get_text(inputTA);
            Serial.printf("ENTER pressed, text='%s', field=%d\n", text, editingField);

            if (editingField == 0) {
                // SSID entered, move to password
                strncpy(wifiSSID, text, MAX_SSID_LEN - 1);
                wifiSSID[MAX_SSID_LEN - 1] = '\0';
                Serial.printf("SSID saved: '%s', moving to password\n", wifiSSID);
                editingField = 1;
                createWiFiAddMenu(wifiSSID);  // Refresh with SSID shown
            } else {
                // Password entered, save network
                strncpy(wifiPass, text, MAX_PASS_LEN - 1);
                wifiPass[MAX_PASS_LEN - 1] = '\0';
                Serial.printf("Password saved: '%s'\n", wifiPass);

                if (strlen(wifiSSID) > 0) {
                    saveWiFiNetwork(wifiSSID, wifiPass);
                    playHapticClick();
                } else {
                    Serial.println("ERROR: SSID is empty!");
                }

                // Go back to WiFi list
                wifiSSID[0] = '\0';
                wifiPass[0] = '\0';
                currentMenu = MENU_WIFI_LIST;
                selectedIndex = 0;
                scrollOffset = 0;
                createWiFiListMenu();
            }
        }
        // Note: 'q' is NOT used for cancel in text input - user might need to type it!
        // Use long-press rotary button to cancel (handled in main loop)
        return;
    }

    // Navigation - 'q' to go back (not backspace - that's for text delete)
    if (key == 'q') {
        goBack();
    } else if (key == '\n' || key == '\r') {
        // Select
        switch (currentMenu) {
            case MENU_MAIN:
                handleMainMenuSelect();
                break;
            case MENU_DISPLAY:
                handleDisplaySelect();
                break;
            case MENU_WIFI_LIST:
                handleWiFiListSelect();
                break;
            case MENU_WIFI_SCAN:
                handleWiFiScanSelect();
                break;
            case MENU_SERVER_LOCAL:
            case MENU_SERVER_REMOTE:
                handleServerSelect();
                break;
            case MENU_SYSTEM:
                handleSystemSelect();
                break;
            case MENU_ABOUT:
                goBack();
                break;
            default:
                break;
        }
    }
}

void settingsUIHandleRotary(int direction) {
    if (direction == 0) {
        // Click - same as Enter
        playHapticClick();
        settingsUIHandleKey('\n');
        return;
    }

    // Haptic feedback for scroll
    playHapticTick();

    // Adjust values in display menu (brightness=1, theme=2)
    if (currentMenu == MENU_DISPLAY && selectedIndex > 0) {
        handleDisplayAdjust(direction);
        return;
    }

    // Adjust values in system menu (volume=2, haptic intensity=4)
    if (currentMenu == MENU_SYSTEM && (selectedIndex == 2 || selectedIndex == 4)) {
        handleSystemAdjust(direction);
        return;
    }

    // Navigate menu
    int maxItems = 0;
    switch (currentMenu) {
        case MENU_MAIN: maxItems = 7; break;
        case MENU_DISPLAY: maxItems = 3; break;
        case MENU_WIFI_LIST: maxItems = settings.wifiNetworkCount + 3; break;
        case MENU_WIFI_SCAN: maxItems = scanCount + 1; break;
        case MENU_SERVER_LOCAL:
        case MENU_SERVER_REMOTE: maxItems = 8; break;
        case MENU_SYSTEM: maxItems = 9; break;
        case MENU_ABOUT: maxItems = 1; break;
        default: return;
    }

    if (maxItems == 0) return;

    selectedIndex += direction;
    if (selectedIndex < 0) selectedIndex = maxItems - 1;
    if (selectedIndex >= maxItems) selectedIndex = 0;

    // Handle scrolling
    if (selectedIndex < scrollOffset) {
        scrollOffset = selectedIndex;
    } else if (selectedIndex >= scrollOffset + maxVisibleItems) {
        scrollOffset = selectedIndex - maxVisibleItems + 1;
    }

    // Refresh current menu to update highlight
    switch (currentMenu) {
        case MENU_MAIN: createMainMenu(); break;
        case MENU_DISPLAY: createDisplayMenu(); break;
        case MENU_WIFI_LIST: createWiFiListMenu(); break;
        case MENU_WIFI_SCAN: createWiFiScanMenu(); break;
        case MENU_SERVER_LOCAL: createServerMenu(false); break;
        case MENU_SERVER_REMOTE: createServerMenu(true); break;
        case MENU_SYSTEM: createSystemMenu(); break;
        default: break;
    }
}

void applyThemeToTerminal() {
    extern lv_obj_t *terminalTA;
    extern lv_obj_t *statusBar;

    const ThemeColors_t *theme = getCurrentTheme();

    if (terminalTA) {
        lv_obj_set_style_bg_color(terminalTA, lv_color_hex(theme->background), 0);
        lv_obj_set_style_text_color(terminalTA, lv_color_hex(theme->foreground), 0);
    }

    // Note: statusBar and statusLabel are local to main file
    // They need to be updated separately
}

static void createWiFiEditMenu(int index) {
    // Similar to add menu but pre-filled with existing data
    createWiFiAddMenu(settings.wifiNetworks[index].ssid);
}
