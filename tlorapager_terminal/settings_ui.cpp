/**
 * Settings UI Implementation
 * Full LVGL menu system for T-LoRa Pager Terminal
 */

#include "settings_ui.h"
#include "settings.h"
#include <WiFi.h>
#include <LilyGoLib.h>

// UI state
static MenuState_t currentMenu = MENU_HIDDEN;
static int selectedIndex = 0;
static int scrollOffset = 0;
static int maxVisibleItems = 5;

// Haptic feedback functions
void playHapticClick() {
    if (!settings.hapticEnabled) return;
    instance.drv.setWaveform(0, 1);
    instance.drv.setWaveform(1, 0);
    instance.drv.run();
}

void playHapticTick() {
    if (!settings.hapticEnabled) return;
    instance.drv.setWaveform(0, 10);
    instance.drv.setWaveform(1, 0);
    instance.drv.run();
}

void playHapticBump() {
    if (!settings.hapticEnabled) return;
    instance.drv.setWaveform(0, 47);
    instance.drv.setWaveform(1, 0);
    instance.drv.run();
}

void playHapticDouble() {
    if (!settings.hapticEnabled) return;
    instance.drv.setWaveform(0, 1);
    instance.drv.setWaveform(1, 1);
    instance.drv.setWaveform(2, 0);
    instance.drv.run();
}

// LVGL objects
static lv_obj_t *settingsScreen = NULL;
static lv_obj_t *menuContainer = NULL;
static lv_obj_t *titleLabel = NULL;
static lv_obj_t *menuList = NULL;
static lv_obj_t *menuStatusLabel = NULL;  // Renamed to avoid conflict
static lv_obj_t *inputTA = NULL;

// WiFi scan results
#define MAX_SCAN_RESULTS 20
static String scanSSIDs[MAX_SCAN_RESULTS];
static int32_t scanRSSIs[MAX_SCAN_RESULTS];
static int scanCount = 0;
static bool scanning = false;

// Edit state
static int editingNetworkIndex = -1;
static int editingField = 0;
static char editBuffer[MAX_PASS_LEN];

// Server editing state
static bool editingRemoteServer = false;
static int serverEditField = -1;  // -1=none, 0=host, 1=port, 2=path, 3=username, 4=password

// WiFi input state
static char wifiSSID[MAX_SSID_LEN] = "";
static char wifiPass[MAX_PASS_LEN] = "";

// Forward declarations
static void createMainMenu();
static void createDisplayMenu();
static void createWiFiListMenu();
static void createWiFiScanMenu();
static void createWiFiAddMenu(const char* ssid);
static void createWiFiEditMenu(int index);
static void createServerMenu(bool isRemote);
static void createServerEditMenu(int field);
static void createSystemMenu();
static void createAboutMenu();
static void goBack();

// External references from main file
extern lv_obj_t *terminalScreen;
extern lv_obj_t *terminalTA;

void settingsUIInit() {
    // Settings screen created on demand
}

void settingsUIShow() {
    if (settingsScreen == NULL) {
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
    if (terminalScreen) {
        lv_scr_load(terminalScreen);
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
    menuStatusLabel = NULL;
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

    lv_obj_t *backLabel = lv_label_create(titleBar);
    lv_label_set_text(backLabel, "hold=back");
    lv_obj_set_style_text_color(backLabel, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_font(backLabel, &lv_font_montserrat_10, 0);
    lv_obj_align(backLabel, LV_ALIGN_RIGHT_MID, -10, 0);

    // Status bar
    lv_obj_t *statBar = lv_obj_create(menuContainer);
    lv_obj_set_size(statBar, 480, 20);
    lv_obj_set_pos(statBar, 0, 202);
    lv_obj_set_style_bg_color(statBar, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(statBar, 0, 0);
    lv_obj_remove_flag(statBar, LV_OBJ_FLAG_SCROLLABLE);

    menuStatusLabel = lv_label_create(statBar);
    lv_label_set_text(menuStatusLabel, "Rotate to scroll, click to select");
    lv_obj_set_style_text_color(menuStatusLabel, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(menuStatusLabel, &lv_font_montserrat_10, 0);
    lv_obj_align(menuStatusLabel, LV_ALIGN_LEFT_MID, 5, 0);

    return menuContainer;
}

static lv_obj_t* addMenuItem(lv_obj_t *list, const char* text, const char* value, int index) {
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

    addMenuItem(menuList, "Local Server (SSH)", settings.localServer.enabled ? "ON" : "OFF", 3);
    addMenuItem(menuList, "Remote Server (SSH)", settings.remoteServer.enabled ? "ON" : "OFF", 4);
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

    if (menuStatusLabel) {
        lv_label_set_text(menuStatusLabel, "Rotate to adjust, click Back to return");
    }
}

static void createWiFiListMenu() {
    const int totalItems = settings.wifiNetworkCount + 3;
    createMenuContainer("WIFI NETWORKS", totalItems);
    createMenuList();

    addMenuItem(menuList, "[< Back]", "", 0);
    addMenuItem(menuList, "[~] Scan for Networks", "", 1);

    for (int i = 0; i < settings.wifiNetworkCount && i < MAX_WIFI_NETWORKS; i++) {
        char status[24];
        if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == settings.wifiNetworks[i].ssid) {
            strcpy(status, "Connected");
        } else if (!settings.wifiNetworks[i].enabled) {
            strcpy(status, "Disabled");
        } else {
            strcpy(status, "");
        }
        addMenuItem(menuList, settings.wifiNetworks[i].ssid, status, i + 2);
    }

    addMenuItem(menuList, "[+] Add Network Manually", "", settings.wifiNetworkCount + 2);

    if (menuStatusLabel) {
        lv_label_set_text(menuStatusLabel, "Click network to toggle, long-hold to delete");
    }
}

static void createWiFiScanMenu() {
    const int totalItems = scanning ? 1 : scanCount + 1;
    createMenuContainer(scanning ? "SCANNING..." : "SELECT NETWORK", totalItems);
    createMenuList();

    addMenuItem(menuList, "[< Back]", "", 0);

    if (scanning) {
        lv_obj_t *label = lv_label_create(menuList);
        lv_label_set_text(label, "Scanning for networks...");
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFF00), 0);
    } else if (scanCount == 0) {
        lv_obj_t *label = lv_label_create(menuList);
        lv_label_set_text(label, "No networks found. Try again.");
        lv_obj_set_style_text_color(label, lv_color_hex(0xFF6666), 0);
    } else {
        for (int i = 0; i < scanCount && i < MAX_SCAN_RESULTS; i++) {
            char rssiStr[16];
            snprintf(rssiStr, sizeof(rssiStr), "%ddBm", scanRSSIs[i]);
            addMenuItem(menuList, scanSSIDs[i].c_str(), rssiStr, i + 1);
        }
    }

    if (menuStatusLabel) {
        lv_label_set_text(menuStatusLabel, "Select network to add");
    }
}

static void createTextInputScreen(const char* title, const char* prompt, const char* initial, bool isPassword) {
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

    lv_obj_t *titleLbl = lv_label_create(titleBar);
    lv_label_set_text(titleLbl, title);
    lv_obj_set_style_text_color(titleLbl, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_text_font(titleLbl, &lv_font_montserrat_14, 0);
    lv_obj_align(titleLbl, LV_ALIGN_LEFT_MID, 10, 0);

    // Prompt
    lv_obj_t *promptLbl = lv_label_create(menuContainer);
    lv_label_set_text(promptLbl, prompt);
    lv_obj_set_style_text_color(promptLbl, lv_color_hex(0xFFFF00), 0);
    lv_obj_set_style_text_font(promptLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(promptLbl, 20, 50);

    // Text input
    inputTA = lv_textarea_create(menuContainer);
    lv_obj_set_size(inputTA, 440, 40);
    lv_obj_set_pos(inputTA, 20, 80);
    lv_textarea_set_one_line(inputTA, true);
    lv_textarea_set_password_mode(inputTA, isPassword);
    lv_obj_set_style_bg_color(inputTA, lv_color_hex(0x222222), 0);
    lv_obj_set_style_text_color(inputTA, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_text_font(inputTA, &lv_font_montserrat_14, 0);
    lv_obj_set_style_border_color(inputTA, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_border_width(inputTA, 2, 0);

    if (initial && strlen(initial) > 0) {
        lv_textarea_set_text(inputTA, initial);
    }

    // Instructions
    lv_obj_t *helpLbl = lv_label_create(menuContainer);
    lv_label_set_text(helpLbl, "Type on keyboard, ENTER to save, long-press knob to cancel");
    lv_obj_set_style_text_color(helpLbl, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(helpLbl, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(helpLbl, 20, 140);

    // Status bar
    lv_obj_t *statBar = lv_obj_create(menuContainer);
    lv_obj_set_size(statBar, 480, 20);
    lv_obj_set_pos(statBar, 0, 202);
    lv_obj_set_style_bg_color(statBar, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(statBar, 0, 0);
    lv_obj_remove_flag(statBar, LV_OBJ_FLAG_SCROLLABLE);

    menuStatusLabel = lv_label_create(statBar);
    lv_label_set_text(menuStatusLabel, isPassword ? "Password hidden for security" : "");
    lv_obj_set_style_text_color(menuStatusLabel, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(menuStatusLabel, &lv_font_montserrat_10, 0);
    lv_obj_align(menuStatusLabel, LV_ALIGN_LEFT_MID, 5, 0);
}

static void createWiFiAddMenu(const char* ssid) {
    if (ssid && strlen(ssid) > 0) {
        strncpy(wifiSSID, ssid, MAX_SSID_LEN - 1);
        editingField = 1;  // Skip to password
    } else {
        wifiSSID[0] = '\0';
        editingField = 0;  // Start with SSID
    }
    wifiPass[0] = '\0';

    if (editingField == 0) {
        createTextInputScreen("ADD WIFI", "Enter network name (SSID):", "", false);
    } else {
        char prompt[64];
        snprintf(prompt, sizeof(prompt), "Enter password for '%s':", wifiSSID);
        createTextInputScreen("ADD WIFI", prompt, "", true);
    }
}

static void createServerMenu(bool isRemote) {
    editingRemoteServer = isRemote;
    serverEditField = -1;
    ServerConfig_t *server = isRemote ? &settings.remoteServer : &settings.localServer;
    const int totalItems = 10;
    createMenuContainer(isRemote ? "REMOTE SSH SERVER" : "LOCAL SSH SERVER", totalItems);
    createMenuList();

    addMenuItem(menuList, "[< Back]", "", 0);
    addMenuItem(menuList, "Enabled", server->enabled ? "YES" : "NO", 1);
    addMenuItem(menuList, "Host", server->host, 2);

    char portStr[8];
    snprintf(portStr, sizeof(portStr), "%d", server->port);
    addMenuItem(menuList, "Port", portStr, 3);

    addMenuItem(menuList, "Username", server->username, 4);
    addMenuItem(menuList, "Password", "****", 5);
    addMenuItem(menuList, "SSL/TLS", server->useSSL ? "YES" : "NO", 6);
    addMenuItem(menuList, "", "", 7);  // Spacer
    addMenuItem(menuList, "[Test Connection]", "", 8);
    addMenuItem(menuList, "[Connect Now]", "", 9);

    if (menuStatusLabel) {
        lv_label_set_text(menuStatusLabel, "Click to edit field");
    }
}

static void createServerEditMenu(int field) {
    serverEditField = field;
    ServerConfig_t *server = editingRemoteServer ? &settings.remoteServer : &settings.localServer;

    const char* title = editingRemoteServer ? "EDIT REMOTE" : "EDIT LOCAL";
    const char* prompt;
    const char* initial;
    bool isPassword = false;
    char portBuf[8];

    switch (field) {
        case 0:  // Host
            prompt = "Enter server hostname or IP:";
            initial = server->host;
            break;
        case 1:  // Port
            prompt = "Enter port number (e.g., 22):";
            snprintf(portBuf, sizeof(portBuf), "%d", server->port);
            initial = portBuf;
            break;
        case 2:  // Username
            prompt = "Enter SSH username:";
            initial = server->username;
            break;
        case 3:  // Password
            prompt = "Enter SSH password:";
            initial = server->password;
            isPassword = true;
            break;
        default:
            return;
    }

    createTextInputScreen(title, prompt, initial, isPassword);
}

static void createSystemMenu() {
    const int totalItems = 9;
    createMenuContainer("SYSTEM SETTINGS", totalItems);
    createMenuList();

    addMenuItem(menuList, "[< Back]", "", 0);

    addMenuItem(menuList, "Sound", settings.soundEnabled ? "ON" : "OFF", 1);
    char volStr[8];
    snprintf(volStr, sizeof(volStr), "%d%%", settings.volume);
    addMenuItem(menuList, "Volume", volStr, 2);

    addMenuItem(menuList, "Haptic Feedback", settings.hapticEnabled ? "ON" : "OFF", 3);
    char hapStr[8];
    snprintf(hapStr, sizeof(hapStr), "%d%%", settings.hapticIntensity);
    addMenuItem(menuList, "Haptic Intensity", hapStr, 4);

    addMenuItem(menuList, "Auto-connect WiFi", settings.wifiAutoConnect ? "ON" : "OFF", 5);
    addMenuItem(menuList, "Prefer Remote Server", settings.preferRemote ? "ON" : "OFF", 6);

    addMenuItem(menuList, "[Reset All Settings]", "", 7);
    addMenuItem(menuList, "[Restart Device]", "", 8);

    if (menuStatusLabel) {
        lv_label_set_text(menuStatusLabel, "Rotate to adjust values, click to toggle");
    }
}

static void createAboutMenu() {
    createMenuContainer("ABOUT", 1);
    createMenuList();

    addMenuItem(menuList, "[< Back]", "", 0);

    lv_obj_t *info = lv_label_create(menuList);
    lv_label_set_text(info,
        "\nT-LoRa Pager SSH Terminal v1.0\n"
        "Native SSH client using LibSSH\n\n"
        "Hardware:\n"
        "  ESP32-S3 @ 240MHz\n"
        "  Display: 480x222 ST7796\n"
        "  Radio: SX1262 LoRa\n"
        "  Keyboard: TCA8418 QWERTY"
    );
    lv_obj_set_style_text_color(info, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(info, &lv_font_montserrat_12, 0);

    if (menuStatusLabel) {
        lv_label_set_text(menuStatusLabel, "");
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
        case 0:
            settingsUIHide();
            break;
        case 1:
            currentMenu = MENU_DISPLAY;
            selectedIndex = 0;
            createDisplayMenu();
            break;
        case 2:
            currentMenu = MENU_WIFI_LIST;
            selectedIndex = 0;
            createWiFiListMenu();
            break;
        case 3:
            currentMenu = MENU_SERVER_LOCAL;
            selectedIndex = 0;
            createServerMenu(false);
            break;
        case 4:
            currentMenu = MENU_SERVER_REMOTE;
            selectedIndex = 0;
            createServerMenu(true);
            break;
        case 5:
            currentMenu = MENU_SYSTEM;
            selectedIndex = 0;
            createSystemMenu();
            break;
        case 6:
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
}

static void handleDisplayAdjust(int direction) {
    if (selectedIndex == 1) {
        int newBrightness = settings.brightness + (direction * 25);
        if (newBrightness < 10) newBrightness = 10;
        if (newBrightness > 255) newBrightness = 255;
        settings.brightness = newBrightness;
        instance.setBrightness(settings.brightness);
        settingsSave();
        createDisplayMenu();
    } else if (selectedIndex == 2) {
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
        case 1:
            settings.soundEnabled = !settings.soundEnabled;
            settingsSave();
            createSystemMenu();
            break;
        case 2:
            {
                int newVol = settings.volume + (direction * 10);
                if (newVol < 0) newVol = 0;
                if (newVol > 100) newVol = 100;
                settings.volume = newVol;
                settingsSave();
                createSystemMenu();
            }
            break;
        case 3:
            settings.hapticEnabled = !settings.hapticEnabled;
            settingsSave();
            playHapticClick();
            createSystemMenu();
            break;
        case 4:
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
        case 5:
            settings.wifiAutoConnect = !settings.wifiAutoConnect;
            settingsSave();
            createSystemMenu();
            break;
        case 6:
            settings.preferRemote = !settings.preferRemote;
            settingsSave();
            createSystemMenu();
            break;
    }
}

static void handleSystemSelect() {
    playHapticClick();
    switch (selectedIndex) {
        case 0:
            goBack();
            break;
        case 1:
        case 3:
        case 5:
        case 6:
            handleSystemAdjust(1);
            break;
        case 7:
            playHapticDouble();
            settingsReset();
            settingsSave();
            createSystemMenu();
            break;
        case 8:
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

static void deleteWiFiNetwork(int index) {
    if (index < 0 || index >= settings.wifiNetworkCount) return;

    for (int i = index; i < settings.wifiNetworkCount - 1; i++) {
        memcpy(&settings.wifiNetworks[i], &settings.wifiNetworks[i + 1], sizeof(WiFiNetwork_t));
    }
    settings.wifiNetworkCount--;
    settingsSave();
    Serial.printf("Deleted WiFi network at index %d\n", index);
}

static void handleWiFiListSelect() {
    playHapticClick();
    if (selectedIndex == 0) {
        goBack();
    } else if (selectedIndex == 1) {
        currentMenu = MENU_WIFI_SCAN;
        selectedIndex = 0;
        settingsUIStartWiFiScan();
    } else if (selectedIndex <= settings.wifiNetworkCount + 1) {
        int netIndex = selectedIndex - 2;
        if (netIndex >= 0 && netIndex < settings.wifiNetworkCount) {
            settings.wifiNetworks[netIndex].enabled = !settings.wifiNetworks[netIndex].enabled;
            settingsSave();
            createWiFiListMenu();
        }
    } else {
        currentMenu = MENU_WIFI_ADD;
        selectedIndex = 0;
        editingNetworkIndex = -1;
        createWiFiAddMenu(NULL);
    }
}

static void handleWiFiScanSelect() {
    playHapticClick();
    if (selectedIndex == 0) {
        goBack();
    } else if (selectedIndex <= scanCount) {
        currentMenu = MENU_WIFI_ADD;
        createWiFiAddMenu(scanSSIDs[selectedIndex - 1].c_str());
    }
}

extern void connectToServer();

static void handleServerSelect() {
    playHapticClick();
    ServerConfig_t *server = editingRemoteServer ? &settings.remoteServer : &settings.localServer;

    switch (selectedIndex) {
        case 0:
            goBack();
            break;
        case 1:
            server->enabled = !server->enabled;
            settingsSave();
            createServerMenu(editingRemoteServer);
            break;
        case 2:  // Host
            createServerEditMenu(0);
            break;
        case 3:  // Port
            createServerEditMenu(1);
            break;
        case 4:  // Username
            createServerEditMenu(2);
            break;
        case 5:  // Password
            createServerEditMenu(3);
            break;
        case 6:  // SSL toggle
            server->useSSL = !server->useSSL;
            settingsSave();
            createServerMenu(editingRemoteServer);
            break;
        case 8:  // Test
            // TODO: Implement connection test
            if (menuStatusLabel) {
                lv_label_set_text(menuStatusLabel, "Test not implemented yet");
            }
            break;
        case 9:  // Connect now
            settingsUIHide();
            connectToServer();
            break;
    }
}

static void saveWiFiNetwork(const char* ssid, const char* password) {
    Serial.printf("Saving WiFi: SSID='%s'\n", ssid);
    if (settings.wifiNetworkCount < MAX_WIFI_NETWORKS) {
        int idx = settings.wifiNetworkCount;
        strncpy(settings.wifiNetworks[idx].ssid, ssid, MAX_SSID_LEN - 1);
        strncpy(settings.wifiNetworks[idx].password, password, MAX_PASS_LEN - 1);
        settings.wifiNetworks[idx].enabled = true;
        settings.wifiNetworkCount++;
        settingsSave();
        Serial.printf("Network saved at index %d\n", idx);
    }
}

void settingsUICancelInput() {
    Serial.println("Input cancelled");
    playHapticBump();
    wifiSSID[0] = '\0';
    wifiPass[0] = '\0';
    editingField = 0;
    serverEditField = -1;

    if (currentMenu == MENU_WIFI_ADD || currentMenu == MENU_WIFI_EDIT) {
        currentMenu = MENU_WIFI_LIST;
        selectedIndex = 0;
        scrollOffset = 0;
        createWiFiListMenu();
    } else if (serverEditField >= 0) {
        serverEditField = -1;
        createServerMenu(editingRemoteServer);
    } else {
        goBack();
    }
}

void settingsUIHandleKey(char key) {
    // Handle text input for WiFi add/edit
    if (inputTA && (currentMenu == MENU_WIFI_ADD || currentMenu == MENU_WIFI_EDIT)) {
        if (key >= 32 && key < 127) {
            lv_textarea_add_char(inputTA, key);
        } else if (key == '\b' || key == 127 || key == 8) {
            lv_textarea_delete_char(inputTA);
        } else if (key == '\n' || key == '\r') {
            const char* text = lv_textarea_get_text(inputTA);

            if (editingField == 0) {
                strncpy(wifiSSID, text, MAX_SSID_LEN - 1);
                wifiSSID[MAX_SSID_LEN - 1] = '\0';
                editingField = 1;
                createWiFiAddMenu(wifiSSID);
            } else {
                strncpy(wifiPass, text, MAX_PASS_LEN - 1);
                wifiPass[MAX_PASS_LEN - 1] = '\0';

                if (strlen(wifiSSID) > 0) {
                    saveWiFiNetwork(wifiSSID, wifiPass);
                    playHapticClick();
                }

                wifiSSID[0] = '\0';
                wifiPass[0] = '\0';
                editingField = 0;
                currentMenu = MENU_WIFI_LIST;
                selectedIndex = 0;
                scrollOffset = 0;
                createWiFiListMenu();
            }
        }
        return;
    }

    // Handle text input for server editing
    if (inputTA && serverEditField >= 0) {
        if (key >= 32 && key < 127) {
            lv_textarea_add_char(inputTA, key);
        } else if (key == '\b' || key == 127 || key == 8) {
            lv_textarea_delete_char(inputTA);
        } else if (key == '\n' || key == '\r') {
            const char* text = lv_textarea_get_text(inputTA);
            ServerConfig_t *server = editingRemoteServer ? &settings.remoteServer : &settings.localServer;

            switch (serverEditField) {
                case 0:  // Host
                    strncpy(server->host, text, MAX_HOST_LEN - 1);
                    break;
                case 1:  // Port
                    server->port = atoi(text);
                    if (server->port == 0) server->port = 22;
                    break;
                case 2:  // Username
                    strncpy(server->username, text, 31);
                    break;
                case 3:  // Password
                    strncpy(server->password, text, 31);
                    break;
            }
            settingsSave();
            playHapticClick();

            serverEditField = -1;
            MenuState_t targetMenu = editingRemoteServer ? MENU_SERVER_REMOTE : MENU_SERVER_LOCAL;
            currentMenu = targetMenu;
            selectedIndex = 0;
            scrollOffset = 0;
            createServerMenu(editingRemoteServer);
        }
        return;
    }

    // Navigation
    if (key == 'q') {
        goBack();
    } else if (key == '\n' || key == '\r') {
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
        playHapticClick();
        settingsUIHandleKey('\n');
        return;
    }

    playHapticTick();

    // Adjust values in display menu
    if (currentMenu == MENU_DISPLAY && selectedIndex > 0) {
        handleDisplayAdjust(direction);
        return;
    }

    // Adjust values in system menu
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
        case MENU_SERVER_REMOTE: maxItems = 10; break;
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

    // Refresh current menu
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
    const ThemeColors_t *theme = getCurrentTheme();

    if (terminalTA) {
        lv_obj_set_style_bg_color(terminalTA, lv_color_hex(theme->background), 0);
        lv_obj_set_style_text_color(terminalTA, lv_color_hex(theme->foreground), 0);
    }

    if (terminalScreen) {
        lv_obj_set_style_bg_color(terminalScreen, lv_color_hex(theme->background), 0);
    }
}

static void createWiFiEditMenu(int index) {
    createWiFiAddMenu(settings.wifiNetworks[index].ssid);
}
