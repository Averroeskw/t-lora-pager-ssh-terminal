/**
 * Settings Implementation
 */

#include "settings.h"

// Global instances
Settings_t settings;
Preferences prefs;

// Theme color definitions
const ThemeColors_t themeColors[THEME_COUNT] = {
    // THEME_GREEN_ON_BLACK - Classic terminal
    {0x000000, 0x00FF00, 0x00AA00, 0x222222, "Green Terminal"},
    // THEME_AMBER_ON_BLACK - Retro amber
    {0x000000, 0xFFBF00, 0xCC9900, 0x1A1A00, "Amber Retro"},
    // THEME_WHITE_ON_BLACK - High contrast
    {0x000000, 0xFFFFFF, 0xAAAAAA, 0x333333, "High Contrast"},
    // THEME_BLACK_ON_WHITE - Light mode
    {0xFFFFFF, 0x000000, 0x666666, 0xEEEEEE, "Light Mode"},
    // THEME_CYAN_ON_BLACK - Cyan terminal
    {0x000000, 0x00FFFF, 0x00AAAA, 0x002222, "Cyan Terminal"},
};

const ThemeColors_t* getCurrentTheme() {
    if (settings.theme >= THEME_COUNT) {
        settings.theme = THEME_GREEN_ON_BLACK;
    }
    return &themeColors[settings.theme];
}

uint32_t settingsCalculateChecksum(Settings_t *s) {
    uint32_t sum = 0;
    uint8_t *data = (uint8_t*)s;
    // Exclude checksum field from calculation
    size_t len = sizeof(Settings_t) - sizeof(uint32_t);
    for (size_t i = 0; i < len; i++) {
        sum += data[i] * (i + 1);
    }
    return sum ^ 0xDEADBEEF;
}

void settingsReset() {
    memset(&settings, 0, sizeof(Settings_t));

    // Set version
    settings.version = SETTINGS_VERSION;

    // Display defaults
    settings.brightness = 200;
    settings.theme = THEME_GREEN_ON_BLACK;

    // WiFi defaults
    settings.wifiNetworkCount = 1;
    strcpy(settings.wifiNetworks[0].ssid, "A & A +a Casa");
    strcpy(settings.wifiNetworks[0].password, "A69693969a");
    settings.wifiNetworks[0].enabled = true;
    settings.wifiAutoConnect = true;

    // Local SSH server (LAN)
    strcpy(settings.localServer.host, "192.168.8.141");
    settings.localServer.port = 22;  // SSH port
    strcpy(settings.localServer.path, "");  // Not used for SSH
    strcpy(settings.localServer.username, "archie");
    strcpy(settings.localServer.password, "archie");
    settings.localServer.useSSL = false;  // Not used for SSH
    settings.localServer.enabled = true;

    // Remote SSH server (Tailscale)
    strcpy(settings.remoteServer.host, "100.107.239.11");  // Tailscale IP
    settings.remoteServer.port = 22;  // SSH port
    strcpy(settings.remoteServer.path, "");  // Not used for SSH
    strcpy(settings.remoteServer.username, "archie");
    strcpy(settings.remoteServer.password, "archie");
    settings.remoteServer.useSSL = false;  // Not used for SSH
    settings.remoteServer.enabled = true;

    settings.preferRemote = false;  // Use local first

    // System defaults - Sound
    settings.soundEnabled = true;
    settings.volume = 50;

    // System defaults - Haptic
    settings.hapticEnabled = true;
    settings.hapticIntensity = 80;

    settings.checksum = settingsCalculateChecksum(&settings);
}

void settingsInit() {
    prefs.begin("tlora-term", false);
    settingsLoad();
}

void settingsLoad() {
    size_t len = prefs.getBytesLength("settings");

    if (len != sizeof(Settings_t)) {
        Serial.println("Settings: Size mismatch, using defaults");
        settingsReset();
        settingsSave();
        return;
    }

    prefs.getBytes("settings", &settings, sizeof(Settings_t));

    // Check version - force reset if outdated
    if (settings.version != SETTINGS_VERSION) {
        Serial.printf("Settings: Version mismatch (%d != %d), resetting\n", settings.version, SETTINGS_VERSION);
        settingsReset();
        settingsSave();
        return;
    }

    // Validate checksum
    uint32_t calculated = settingsCalculateChecksum(&settings);
    if (calculated != settings.checksum) {
        Serial.println("Settings: Checksum mismatch, resetting");
        settingsReset();
        settingsSave();
        return;
    }

    Serial.printf("Settings: Loaded (v%d, brightness=%d, wifi=%s)\n",
                  settings.version, settings.brightness,
                  settings.wifiNetworkCount > 0 ? settings.wifiNetworks[0].ssid : "none");
}

void settingsSave() {
    settings.checksum = settingsCalculateChecksum(&settings);
    prefs.putBytes("settings", &settings, sizeof(Settings_t));
    Serial.println("Settings: Saved");
}
