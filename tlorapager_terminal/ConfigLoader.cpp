#include "ConfigLoader.h"
#include <LittleFS.h>
#include <tinyxml2.h>

using namespace tinyxml2;

// Global instance
ConfigLoader configLoader;

ConfigLoader::ConfigLoader() : _loaded(false), _nvsOpen(false) {
    setDefaults();
}

ConfigLoader::~ConfigLoader() {
    if (_nvsOpen) {
        _prefs.end();
    }
}

void ConfigLoader::setDefaults() {
    // Wi-Fi defaults (empty - must be set via NVS or XML)
    _config.wifi.ssid = "";
    _config.wifi.password = "";

    // Gateway defaults
    _config.gateway.host = "192.168.1.100";
    _config.gateway.port = 7681;
    _config.gateway.path = "/ws";
    _config.gateway.useSsl = false;
    _config.gateway.sni = "";
    _config.gateway.connectTimeoutMs = 4000;
    _config.gateway.reconnectDelayMs = 800;
    _config.gateway.maxReconnectDelayMs = 5000;
    _config.gateway.pingIntervalMs = 15000;

    // Terminal defaults (80x18 as per README)
    _config.terminal.cols = 80;
    _config.terminal.rows = 18;
    _config.terminal.scrollbackLines = 0;
    _config.terminal.fontName = "mono";
    _config.terminal.fontSize = 14;

    // Input defaults
    _config.input.keyboard.keymapFile = "/config/keymaps/us_qwerty.xml";
    _config.input.keyboard.debounceMs = 15;
    _config.input.encoder.pressSendsEnter = true;
    _config.input.encoder.rotateScrollEnabled = false;
    _config.input.encoder.rotateStepLines = 1;

    // Haptics defaults
    _config.haptics.enabled = true;
    _config.haptics.keypressMs = 8;
    _config.haptics.bellMs = 40;

    // UI defaults
    _config.ui.statusBarEnabled = true;
    _config.ui.themeFile = "/config/themes/nasa_minimal.xml";

    // Logging defaults
    _config.logging.serialBaud = 115200;
    _config.logging.debugWebSocket = false;
    _config.logging.debugKeyboard = false;

    // Theme defaults (nasa_minimal)
    _config.theme.name = "nasa_minimal";
    uint8_t bg[] = {0, 0, 0};
    uint8_t fg[] = {230, 230, 230};
    uint8_t muted[] = {140, 140, 140};
    uint8_t ok[] = {80, 220, 160};
    uint8_t warn[] = {240, 200, 80};
    uint8_t err[] = {255, 90, 90};
    uint8_t statusBg[] = {20, 20, 20};
    uint8_t statusFg[] = {220, 220, 220};
    memcpy(_config.theme.colors.bg, bg, 3);
    memcpy(_config.theme.colors.fg, fg, 3);
    memcpy(_config.theme.colors.muted, muted, 3);
    memcpy(_config.theme.colors.ok, ok, 3);
    memcpy(_config.theme.colors.warn, warn, 3);
    memcpy(_config.theme.colors.err, err, 3);
    memcpy(_config.theme.colors.statusBg, statusBg, 3);
    memcpy(_config.theme.colors.statusFg, statusFg, 3);
    _config.theme.cursor.style = "block";
    _config.theme.cursor.blink = true;
    _config.theme.selectionInvert = true;
    _config.theme.statusBar.heightPx = 18;
    _config.theme.statusBar.icons = true;
    _config.theme.statusBar.showWifi = true;
    _config.theme.statusBar.showWebSocket = true;
    _config.theme.statusBar.showModifiers = true;
}

bool ConfigLoader::begin() {
    // Initialize LittleFS
    if (!LittleFS.begin(true)) {  // true = format on fail
        Serial.println("[ConfigLoader] LittleFS mount failed!");
        return false;
    }
    Serial.println("[ConfigLoader] LittleFS mounted");

    // Initialize NVS
    if (_prefs.begin(NVS_NAMESPACE, false)) {  // false = read-write
        _nvsOpen = true;
        Serial.println("[ConfigLoader] NVS initialized");
    } else {
        Serial.println("[ConfigLoader] NVS init failed!");
    }

    return true;
}

String ConfigLoader::readFile(const char* path) {
    File file = LittleFS.open(path, "r");
    if (!file) {
        Serial.printf("[ConfigLoader] Failed to open: %s\n", path);
        return "";
    }

    String content = file.readString();
    file.close();
    return content;
}

bool ConfigLoader::loadConfig(const char* path) {
    Serial.printf("[ConfigLoader] Loading config: %s\n", path);

    String xml = readFile(path);
    if (xml.isEmpty()) {
        Serial.println("[ConfigLoader] Config file empty or not found, using defaults");
        _loaded = true;
        return true;
    }

    if (!parseMainConfig(xml.c_str())) {
        Serial.println("[ConfigLoader] Failed to parse main config");
        return false;
    }

    // Try to load Wi-Fi from NVS first (overrides XML)
    if (_nvsOpen) {
        loadWifiFromNvs();
    }

    // Load theme if specified
    if (_config.ui.themeFile.length() > 0) {
        loadTheme();
    }

    // Load keymap if specified
    if (_config.input.keyboard.keymapFile.length() > 0) {
        loadKeymap();
    }

    _loaded = true;
    Serial.println("[ConfigLoader] Config loaded successfully");
    return true;
}

bool ConfigLoader::parseMainConfig(const char* xml) {
    XMLDocument doc;
    if (doc.Parse(xml) != XML_SUCCESS) {
        Serial.printf("[ConfigLoader] XML parse error: %s\n", doc.ErrorStr());
        return false;
    }

    XMLElement* root = doc.FirstChildElement("tloraTerminalConfig");
    if (!root) {
        Serial.println("[ConfigLoader] Missing root element");
        return false;
    }

    // Parse wifi section
    XMLElement* wifi = root->FirstChildElement("wifi");
    if (wifi) {
        XMLElement* el;
        if ((el = wifi->FirstChildElement("ssid"))) _config.wifi.ssid = el->GetText() ? el->GetText() : "";
        if ((el = wifi->FirstChildElement("password"))) _config.wifi.password = el->GetText() ? el->GetText() : "";
    }

    // Parse gateway section
    XMLElement* gateway = root->FirstChildElement("gateway");
    if (gateway) {
        XMLElement* el;
        if ((el = gateway->FirstChildElement("host"))) _config.gateway.host = el->GetText() ? el->GetText() : "";
        if ((el = gateway->FirstChildElement("port"))) el->QueryUnsignedText((unsigned int*)&_config.gateway.port);
        if ((el = gateway->FirstChildElement("path"))) _config.gateway.path = el->GetText() ? el->GetText() : "";
        if ((el = gateway->FirstChildElement("useSsl"))) _config.gateway.useSsl = strcmp(el->GetText(), "true") == 0;
        if ((el = gateway->FirstChildElement("sni"))) _config.gateway.sni = el->GetText() ? el->GetText() : "";
        if ((el = gateway->FirstChildElement("connectTimeoutMs"))) el->QueryUnsignedText(&_config.gateway.connectTimeoutMs);
        if ((el = gateway->FirstChildElement("reconnectDelayMs"))) el->QueryUnsignedText(&_config.gateway.reconnectDelayMs);
        if ((el = gateway->FirstChildElement("maxReconnectDelayMs"))) el->QueryUnsignedText(&_config.gateway.maxReconnectDelayMs);
        if ((el = gateway->FirstChildElement("pingIntervalMs"))) el->QueryUnsignedText(&_config.gateway.pingIntervalMs);
    }

    // Parse terminal section
    XMLElement* terminal = root->FirstChildElement("terminal");
    if (terminal) {
        XMLElement* el;
        unsigned int tmp;
        if ((el = terminal->FirstChildElement("cols"))) { el->QueryUnsignedText(&tmp); _config.terminal.cols = tmp; }
        if ((el = terminal->FirstChildElement("rows"))) { el->QueryUnsignedText(&tmp); _config.terminal.rows = tmp; }
        if ((el = terminal->FirstChildElement("scrollbackLines"))) { el->QueryUnsignedText(&tmp); _config.terminal.scrollbackLines = tmp; }

        XMLElement* font = terminal->FirstChildElement("font");
        if (font) {
            if ((el = font->FirstChildElement("name"))) _config.terminal.fontName = el->GetText() ? el->GetText() : "";
            if ((el = font->FirstChildElement("size"))) { el->QueryUnsignedText(&tmp); _config.terminal.fontSize = tmp; }
        }
    }

    // Parse input section
    XMLElement* input = root->FirstChildElement("input");
    if (input) {
        XMLElement* keyboard = input->FirstChildElement("keyboard");
        if (keyboard) {
            XMLElement* el;
            unsigned int tmp;
            if ((el = keyboard->FirstChildElement("keymapFile"))) _config.input.keyboard.keymapFile = el->GetText() ? el->GetText() : "";
            if ((el = keyboard->FirstChildElement("debounceMs"))) { el->QueryUnsignedText(&tmp); _config.input.keyboard.debounceMs = tmp; }
        }

        XMLElement* encoder = input->FirstChildElement("encoder");
        if (encoder) {
            XMLElement* el;
            unsigned int tmp;
            if ((el = encoder->FirstChildElement("pressSendsEnter"))) _config.input.encoder.pressSendsEnter = strcmp(el->GetText(), "true") == 0;
            if ((el = encoder->FirstChildElement("rotateScrollEnabled"))) _config.input.encoder.rotateScrollEnabled = strcmp(el->GetText(), "true") == 0;
            if ((el = encoder->FirstChildElement("rotateStepLines"))) { el->QueryUnsignedText(&tmp); _config.input.encoder.rotateStepLines = tmp; }
        }
    }

    // Parse haptics section
    XMLElement* haptics = root->FirstChildElement("haptics");
    if (haptics) {
        XMLElement* el;
        unsigned int tmp;
        if ((el = haptics->FirstChildElement("enabled"))) _config.haptics.enabled = strcmp(el->GetText(), "true") == 0;
        if ((el = haptics->FirstChildElement("keypressMs"))) { el->QueryUnsignedText(&tmp); _config.haptics.keypressMs = tmp; }
        if ((el = haptics->FirstChildElement("bellMs"))) { el->QueryUnsignedText(&tmp); _config.haptics.bellMs = tmp; }
    }

    // Parse ui section
    XMLElement* ui = root->FirstChildElement("ui");
    if (ui) {
        XMLElement* el;
        if ((el = ui->FirstChildElement("statusBarEnabled"))) _config.ui.statusBarEnabled = strcmp(el->GetText(), "true") == 0;
        if ((el = ui->FirstChildElement("themeFile"))) _config.ui.themeFile = el->GetText() ? el->GetText() : "";
    }

    // Parse logging section
    XMLElement* logging = root->FirstChildElement("logging");
    if (logging) {
        XMLElement* el;
        if ((el = logging->FirstChildElement("serialBaud"))) el->QueryUnsignedText(&_config.logging.serialBaud);
        if ((el = logging->FirstChildElement("debugWebSocket"))) _config.logging.debugWebSocket = strcmp(el->GetText(), "true") == 0;
        if ((el = logging->FirstChildElement("debugKeyboard"))) _config.logging.debugKeyboard = strcmp(el->GetText(), "true") == 0;
    }

    return true;
}

bool ConfigLoader::loadGatewayProfile(const char* profileName) {
    String path = String("/config/profiles/") + profileName + ".xml";
    Serial.printf("[ConfigLoader] Loading profile: %s\n", path.c_str());

    String xml = readFile(path.c_str());
    if (xml.isEmpty()) {
        return false;
    }

    if (!parseGatewayProfile(xml.c_str())) {
        return false;
    }

    // Save as last used profile
    if (_nvsOpen) {
        saveLastProfile(profileName);
    }

    return true;
}

bool ConfigLoader::parseGatewayProfile(const char* xml) {
    XMLDocument doc;
    if (doc.Parse(xml) != XML_SUCCESS) {
        return false;
    }

    XMLElement* root = doc.FirstChildElement("gatewayProfile");
    if (!root) {
        return false;
    }

    XMLElement* el;
    if ((el = root->FirstChildElement("host"))) _config.gateway.host = el->GetText() ? el->GetText() : "";
    if ((el = root->FirstChildElement("port"))) el->QueryUnsignedText((unsigned int*)&_config.gateway.port);
    if ((el = root->FirstChildElement("path"))) _config.gateway.path = el->GetText() ? el->GetText() : "";
    if ((el = root->FirstChildElement("useSsl"))) _config.gateway.useSsl = strcmp(el->GetText(), "true") == 0;
    if ((el = root->FirstChildElement("sni"))) _config.gateway.sni = el->GetText() ? el->GetText() : "";

    return true;
}

bool ConfigLoader::loadTheme() {
    if (_config.ui.themeFile.isEmpty()) {
        return false;
    }

    // Ensure path starts with /
    String path = _config.ui.themeFile;
    if (!path.startsWith("/")) {
        path = "/" + path;
    }

    Serial.printf("[ConfigLoader] Loading theme: %s\n", path.c_str());

    String xml = readFile(path.c_str());
    if (xml.isEmpty()) {
        return false;
    }

    return parseTheme(xml.c_str());
}

bool ConfigLoader::parseTheme(const char* xml) {
    XMLDocument doc;
    if (doc.Parse(xml) != XML_SUCCESS) {
        return false;
    }

    XMLElement* root = doc.FirstChildElement("theme");
    if (!root) {
        return false;
    }

    const char* name = root->Attribute("name");
    if (name) _config.theme.name = name;

    // Parse colors
    XMLElement* colors = root->FirstChildElement("colors");
    if (colors) {
        auto parseColor = [](XMLElement* el, uint8_t* rgb) {
            if (el) {
                int r = 0, g = 0, b = 0;
                el->QueryIntAttribute("r", &r);
                el->QueryIntAttribute("g", &g);
                el->QueryIntAttribute("b", &b);
                rgb[0] = r; rgb[1] = g; rgb[2] = b;
            }
        };

        parseColor(colors->FirstChildElement("bg"), _config.theme.colors.bg);
        parseColor(colors->FirstChildElement("fg"), _config.theme.colors.fg);
        parseColor(colors->FirstChildElement("muted"), _config.theme.colors.muted);
        parseColor(colors->FirstChildElement("ok"), _config.theme.colors.ok);
        parseColor(colors->FirstChildElement("warn"), _config.theme.colors.warn);
        parseColor(colors->FirstChildElement("err"), _config.theme.colors.err);
        parseColor(colors->FirstChildElement("statusBg"), _config.theme.colors.statusBg);
        parseColor(colors->FirstChildElement("statusFg"), _config.theme.colors.statusFg);
    }

    // Parse terminal section
    XMLElement* terminal = root->FirstChildElement("terminal");
    if (terminal) {
        XMLElement* cursor = terminal->FirstChildElement("cursor");
        if (cursor) {
            XMLElement* el;
            if ((el = cursor->FirstChildElement("style"))) _config.theme.cursor.style = el->GetText() ? el->GetText() : "";
            if ((el = cursor->FirstChildElement("blink"))) _config.theme.cursor.blink = strcmp(el->GetText(), "true") == 0;
        }

        XMLElement* selection = terminal->FirstChildElement("selection");
        if (selection) {
            XMLElement* el;
            if ((el = selection->FirstChildElement("invert"))) _config.theme.selectionInvert = strcmp(el->GetText(), "true") == 0;
        }
    }

    // Parse statusBar section
    XMLElement* statusBar = root->FirstChildElement("statusBar");
    if (statusBar) {
        XMLElement* el;
        unsigned int tmp;
        if ((el = statusBar->FirstChildElement("heightPx"))) { el->QueryUnsignedText(&tmp); _config.theme.statusBar.heightPx = tmp; }
        if ((el = statusBar->FirstChildElement("icons"))) _config.theme.statusBar.icons = strcmp(el->GetText(), "true") == 0;
        if ((el = statusBar->FirstChildElement("showWifi"))) _config.theme.statusBar.showWifi = strcmp(el->GetText(), "true") == 0;
        if ((el = statusBar->FirstChildElement("showWebSocket"))) _config.theme.statusBar.showWebSocket = strcmp(el->GetText(), "true") == 0;
        if ((el = statusBar->FirstChildElement("showModifiers"))) _config.theme.statusBar.showModifiers = strcmp(el->GetText(), "true") == 0;
    }

    return true;
}

bool ConfigLoader::loadKeymap() {
    if (_config.input.keyboard.keymapFile.isEmpty()) {
        return false;
    }

    // Ensure path starts with /
    String path = _config.input.keyboard.keymapFile;
    if (!path.startsWith("/")) {
        path = "/" + path;
    }

    Serial.printf("[ConfigLoader] Loading keymap: %s\n", path.c_str());

    String xml = readFile(path.c_str());
    if (xml.isEmpty()) {
        Serial.println("[ConfigLoader] Keymap file not found, using defaults");
        return false;
    }

    return parseKeymap(xml.c_str());
}

bool ConfigLoader::parseKeymap(const char* xml) {
    XMLDocument doc;
    if (doc.Parse(xml) != XML_SUCCESS) {
        Serial.printf("[ConfigLoader] Keymap XML parse error: %s\n", doc.ErrorStr());
        return false;
    }

    XMLElement* root = doc.FirstChildElement("keymap");
    if (!root) {
        Serial.println("[ConfigLoader] Missing keymap root element");
        return false;
    }

    const char* name = root->Attribute("name");
    if (name) _config.keymap.name = name;

    // Clear existing keys and modifiers
    _config.keymap.keys.clear();
    _config.keymap.modifiers.clear();

    // Parse keys section
    XMLElement* keys = root->FirstChildElement("keys");
    if (keys) {
        for (XMLElement* key = keys->FirstChildElement("key"); key; key = key->NextSiblingElement("key")) {
            KeyMapping km;
            km.code = -1;  // Default: not a control key

            const char* id = key->Attribute("id");
            const char* normal = key->Attribute("normal");
            const char* shift = key->Attribute("shift");

            if (id) km.id = id;
            if (normal) km.normal = normal;
            if (shift) km.shift = shift;

            // Check for code attribute (control keys)
            int code = -1;
            if (key->QueryIntAttribute("code", &code) == XML_SUCCESS) {
                km.code = code;
            }

            _config.keymap.keys.push_back(km);
        }
    }

    // Parse modifiers section
    XMLElement* modifiers = root->FirstChildElement("modifiers");
    if (modifiers) {
        for (XMLElement* mod = modifiers->FirstChildElement("modifier"); mod; mod = mod->NextSiblingElement("modifier")) {
            ModifierDef md;

            const char* id = mod->Attribute("id");
            const char* mode = mod->Attribute("mode");

            if (id) md.id = id;
            if (mode) md.mode = mode;

            _config.keymap.modifiers.push_back(md);
        }
    }

    Serial.printf("[ConfigLoader] Loaded keymap '%s': %d keys, %d modifiers\n",
        _config.keymap.name.c_str(),
        _config.keymap.keys.size(),
        _config.keymap.modifiers.size());

    return true;
}

// NVS Operations
bool ConfigLoader::saveWifiToNvs(const String& ssid, const String& password) {
    if (!_nvsOpen) return false;

    _prefs.putString(NVS_WIFI_SSID, ssid);
    _prefs.putString(NVS_WIFI_PASS, password);

    _config.wifi.ssid = ssid;
    _config.wifi.password = password;

    Serial.println("[ConfigLoader] Wi-Fi credentials saved to NVS");
    return true;
}

bool ConfigLoader::loadWifiFromNvs() {
    if (!_nvsOpen) return false;

    String ssid = _prefs.getString(NVS_WIFI_SSID, "");
    String pass = _prefs.getString(NVS_WIFI_PASS, "");

    if (ssid.length() > 0) {
        _config.wifi.ssid = ssid;
        _config.wifi.password = pass;
        Serial.println("[ConfigLoader] Wi-Fi credentials loaded from NVS");
        return true;
    }

    return false;
}

bool ConfigLoader::saveLastProfile(const String& profileName) {
    if (!_nvsOpen) return false;
    _prefs.putString(NVS_LAST_PROFILE, profileName);
    return true;
}

String ConfigLoader::getLastProfile() {
    if (!_nvsOpen) return "";
    return _prefs.getString(NVS_LAST_PROFILE, "");
}

bool ConfigLoader::clearNvs() {
    if (!_nvsOpen) return false;
    return _prefs.clear();
}

void ConfigLoader::listProfiles() {
    Serial.println("[ConfigLoader] Available profiles:");
    File root = LittleFS.open("/config/profiles");
    if (!root || !root.isDirectory()) {
        Serial.println("  (none found)");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String name = file.name();
            if (name.endsWith(".xml")) {
                name = name.substring(0, name.length() - 4);
                Serial.printf("  - %s\n", name.c_str());
            }
        }
        file = root.openNextFile();
    }
}

void ConfigLoader::printConfig() {
    Serial.println("\n=== Current Configuration ===");
    Serial.printf("Wi-Fi SSID: %s\n", _config.wifi.ssid.c_str());
    Serial.printf("Wi-Fi Pass: %s\n", _config.wifi.password.length() > 0 ? "****" : "(empty)");
    Serial.println();
    Serial.printf("Gateway: %s://%s:%d%s\n",
        _config.gateway.useSsl ? "wss" : "ws",
        _config.gateway.host.c_str(),
        _config.gateway.port,
        _config.gateway.path.c_str());
    Serial.printf("  Connect timeout: %dms\n", _config.gateway.connectTimeoutMs);
    Serial.printf("  Reconnect delay: %d-%dms\n", _config.gateway.reconnectDelayMs, _config.gateway.maxReconnectDelayMs);
    Serial.printf("  Ping interval: %dms\n", _config.gateway.pingIntervalMs);
    Serial.println();
    Serial.printf("Terminal: %dx%d\n", _config.terminal.cols, _config.terminal.rows);
    Serial.printf("  Font: %s @ %d\n", _config.terminal.fontName.c_str(), _config.terminal.fontSize);
    Serial.printf("  Scrollback: %d lines\n", _config.terminal.scrollbackLines);
    Serial.println();
    Serial.printf("Haptics: %s (keypress=%dms, bell=%dms)\n",
        _config.haptics.enabled ? "ON" : "OFF",
        _config.haptics.keypressMs,
        _config.haptics.bellMs);
    Serial.println();
    Serial.printf("Theme: %s\n", _config.theme.name.c_str());
    Serial.printf("  Cursor: %s, blink=%s\n",
        _config.theme.cursor.style.c_str(),
        _config.theme.cursor.blink ? "yes" : "no");
    Serial.println("==============================\n");
}
