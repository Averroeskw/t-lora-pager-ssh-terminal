#ifndef CONFIG_LOADER_H
#define CONFIG_LOADER_H

#include <Arduino.h>
#include <Preferences.h>
#include <vector>

// Configuration structures
struct WifiConfig {
    String ssid;
    String password;
};

struct GatewayConfig {
    String host;
    uint16_t port;
    String path;
    bool useSsl;
    String sni;  // optional SNI for SSL
    uint32_t connectTimeoutMs;
    uint32_t reconnectDelayMs;
    uint32_t maxReconnectDelayMs;
    uint32_t pingIntervalMs;
};

struct TerminalConfig {
    uint16_t cols;
    uint16_t rows;
    uint16_t scrollbackLines;
    String fontName;
    uint8_t fontSize;
};

struct KeyboardConfig {
    String keymapFile;
    uint8_t debounceMs;
};

struct EncoderConfig {
    bool pressSendsEnter;
    bool rotateScrollEnabled;
    uint8_t rotateStepLines;
};

struct InputConfig {
    KeyboardConfig keyboard;
    EncoderConfig encoder;
};

struct HapticsConfig {
    bool enabled;
    uint8_t keypressMs;
    uint8_t bellMs;
};

struct UiConfig {
    bool statusBarEnabled;
    String themeFile;
};

struct LoggingConfig {
    uint32_t serialBaud;
    bool debugWebSocket;
    bool debugKeyboard;
};

struct ThemeColors {
    uint8_t bg[3];
    uint8_t fg[3];
    uint8_t muted[3];
    uint8_t ok[3];
    uint8_t warn[3];
    uint8_t err[3];
    uint8_t statusBg[3];
    uint8_t statusFg[3];
};

struct CursorConfig {
    String style;  // block, underline, bar
    bool blink;
};

struct StatusBarConfig {
    uint8_t heightPx;
    bool icons;
    bool showWifi;
    bool showWebSocket;
    bool showModifiers;
};

struct ThemeConfig {
    String name;
    ThemeColors colors;
    CursorConfig cursor;
    bool selectionInvert;
    StatusBarConfig statusBar;
};

// Keymap structures
struct KeyMapping {
    String id;           // Key identifier (e.g., "A", "1", "COMMA")
    String normal;       // Normal character output
    String shift;        // Shifted character output
    int code;            // ASCII code for control keys (-1 if using normal/shift)
};

struct ModifierDef {
    String id;           // Modifier name (e.g., "SHIFT", "CTRL")
    String mode;         // "oneshot" or "sticky"
};

struct KeymapConfig {
    String name;
    std::vector<KeyMapping> keys;
    std::vector<ModifierDef> modifiers;
};

struct TLoraConfig {
    WifiConfig wifi;
    GatewayConfig gateway;
    TerminalConfig terminal;
    InputConfig input;
    HapticsConfig haptics;
    UiConfig ui;
    LoggingConfig logging;
    ThemeConfig theme;
    KeymapConfig keymap;
};

class ConfigLoader {
public:
    ConfigLoader();
    ~ConfigLoader();

    // Initialize filesystem and NVS
    bool begin();

    // Load main config from LittleFS XML
    bool loadConfig(const char* path = "/config/tlora_terminal_config.xml");

    // Load a gateway profile and merge into current config
    bool loadGatewayProfile(const char* profileName);

    // Load theme from file specified in config
    bool loadTheme();

    // Load keymap from file specified in config
    bool loadKeymap();

    // NVS operations for sensitive data
    bool saveWifiToNvs(const String& ssid, const String& password);
    bool loadWifiFromNvs();
    bool saveLastProfile(const String& profileName);
    String getLastProfile();
    bool clearNvs();

    // List available profiles
    void listProfiles();

    // Access loaded config
    const TLoraConfig& getConfig() const { return _config; }
    TLoraConfig& getConfigMutable() { return _config; }

    // Check if config is loaded
    bool isLoaded() const { return _loaded; }

    // Debug: print current config to Serial
    void printConfig();

private:
    TLoraConfig _config;
    Preferences _prefs;
    bool _loaded;
    bool _nvsOpen;

    // Set defaults
    void setDefaults();

    // Helper to read file content
    String readFile(const char* path);

    // XML parsing helpers (using TinyXML2)
    bool parseMainConfig(const char* xml);
    bool parseGatewayProfile(const char* xml);
    bool parseTheme(const char* xml);
    bool parseKeymap(const char* xml);

    // NVS namespace
    static constexpr const char* NVS_NAMESPACE = "tlora_cfg";
    static constexpr const char* NVS_WIFI_SSID = "wifi_ssid";
    static constexpr const char* NVS_WIFI_PASS = "wifi_pass";
    static constexpr const char* NVS_LAST_PROFILE = "last_profile";
};

// Global config instance
extern ConfigLoader configLoader;

#endif // CONFIG_LOADER_H
