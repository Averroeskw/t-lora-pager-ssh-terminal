/**
 * Settings Management for T-LoRa Pager Terminal
 * Handles persistent storage and settings UI
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <Preferences.h>

// Maximum stored WiFi networks
#define MAX_WIFI_NETWORKS 5
#define MAX_SSID_LEN 32
#define MAX_PASS_LEN 64
#define MAX_HOST_LEN 64
#define MAX_PATH_LEN 32

// Theme definitions
typedef enum {
    THEME_GREEN_ON_BLACK = 0,  // Classic terminal
    THEME_AMBER_ON_BLACK,      // Retro amber
    THEME_WHITE_ON_BLACK,      // High contrast
    THEME_BLACK_ON_WHITE,      // Light mode
    THEME_CYAN_ON_BLACK,       // Cyan terminal
    THEME_COUNT
} Theme_t;

// WiFi network entry
typedef struct {
    char ssid[MAX_SSID_LEN];
    char password[MAX_PASS_LEN];
    bool enabled;
} WiFiNetwork_t;

// Server configuration
typedef struct {
    char host[MAX_HOST_LEN];
    uint16_t port;
    char path[MAX_PATH_LEN];
    char username[32];
    char password[32];
    bool useSSL;
    bool enabled;
} ServerConfig_t;

// Settings version - increment to force reset on structure change
#define SETTINGS_VERSION 12  // SSH support

// Complete settings structure
typedef struct {
    // Version for compatibility check
    uint8_t version;

    // Display settings
    uint8_t brightness;        // 0-255
    Theme_t theme;

    // WiFi networks (priority order)
    WiFiNetwork_t wifiNetworks[MAX_WIFI_NETWORKS];
    uint8_t wifiNetworkCount;
    bool wifiAutoConnect;

    // Server configurations
    ServerConfig_t localServer;   // Local ttyd server
    ServerConfig_t remoteServer;  // Remote ttyd server
    bool preferRemote;            // Try remote first

    // System - Sound
    bool soundEnabled;
    uint8_t volume;              // 0-100

    // System - Haptic
    bool hapticEnabled;
    uint8_t hapticIntensity;     // 0-100

    // Checksum for validation
    uint32_t checksum;
} Settings_t;

// Global settings instance
extern Settings_t settings;
extern Preferences prefs;

// Settings functions
void settingsInit();
void settingsLoad();
void settingsSave();
void settingsReset();
uint32_t settingsCalculateChecksum(Settings_t *s);

// Theme colors
typedef struct {
    uint32_t background;
    uint32_t foreground;
    uint32_t accent;
    uint32_t statusBar;
    const char* name;
} ThemeColors_t;

extern const ThemeColors_t themeColors[THEME_COUNT];

// Get current theme colors
const ThemeColors_t* getCurrentTheme();

#endif // SETTINGS_H
