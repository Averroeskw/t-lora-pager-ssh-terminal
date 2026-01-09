#ifndef CONFIG_LOADER_INTEGRATION_H
#define CONFIG_LOADER_INTEGRATION_H

/*
 * ConfigLoader Integration Guide
 * ==============================
 *
 * This file shows how to integrate ConfigLoader into your tlorapager_terminal.ino
 *
 * STEP 1: Add includes at top of your .ino file
 * ------------------------------------------------
 */

// Add this near the top of tlorapager_terminal.ino:
// #include "ConfigLoader.h"

/*
 * STEP 2: Remove hardcoded values
 * ------------------------------------------------
 * Replace hardcoded values with config lookups.
 *
 * BEFORE:
 *   const char* WIFI_SSID = "YourNetwork";
 *   const char* WIFI_PASS = "YourPassword";
 *   const char* GATEWAY_HOST = "192.168.1.100";
 *   const int GATEWAY_PORT = 7681;
 *   const char* GATEWAY_PATH = "/ws";
 *   const bool USE_SSL = false;
 *
 * AFTER:
 *   // Use configLoader.getConfig().wifi.ssid, etc.
 *
 *
 * STEP 3: Modify setup() function
 * ------------------------------------------------
 */

// Example setup() integration:
/*

void setup() {
    Serial.begin(115200);
    delay(100);

    // Initialize config loader (LittleFS + NVS)
    if (!configLoader.begin()) {
        Serial.println("FATAL: Config system init failed!");
        while (1) delay(1000);
    }

    // Load main config from LittleFS
    configLoader.loadConfig("/config/tlora_terminal_config.xml");

    // Optional: Try loading last-used gateway profile
    String lastProfile = configLoader.getLastProfile();
    if (lastProfile.length() > 0) {
        Serial.printf("Loading last profile: %s\n", lastProfile.c_str());
        configLoader.loadGatewayProfile(lastProfile.c_str());
    }

    // Debug: print loaded config
    configLoader.printConfig();

    // Get config reference for easy access
    const TLoraConfig& cfg = configLoader.getConfig();

    // Initialize Serial with config baud rate
    if (cfg.logging.serialBaud != 115200) {
        Serial.end();
        Serial.begin(cfg.logging.serialBaud);
    }

    // Initialize display (existing code)
    // ...

    // Connect to WiFi using config values
    WiFi.begin(cfg.wifi.ssid.c_str(), cfg.wifi.password.c_str());

    // Build WebSocket URL using config
    String wsUrl = String(cfg.gateway.useSsl ? "wss://" : "ws://")
                 + cfg.gateway.host
                 + ":" + String(cfg.gateway.port)
                 + cfg.gateway.path;

    // ... rest of your setup
}

*/

/*
 * STEP 4: Update references throughout code
 * ------------------------------------------------
 * Replace direct variable access with config lookups:
 *
 *   // Terminal dimensions
 *   int cols = configLoader.getConfig().terminal.cols;  // instead of 80
 *   int rows = configLoader.getConfig().terminal.rows;  // instead of 18
 *
 *   // Haptics
 *   if (configLoader.getConfig().haptics.enabled) {
 *       hapticPulse(configLoader.getConfig().haptics.keypressMs);
 *   }
 *
 *   // Status bar
 *   if (configLoader.getConfig().ui.statusBarEnabled) {
 *       drawStatusBar();
 *   }
 *
 *
 * STEP 5: Optional - Add serial commands for runtime config
 * ------------------------------------------------
 */

// Add to your loop() or a serial handler:
/*

void handleSerialCommands() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if (cmd == "config") {
            configLoader.printConfig();
        }
        else if (cmd == "profiles") {
            configLoader.listProfiles();
        }
        else if (cmd.startsWith("profile ")) {
            String name = cmd.substring(8);
            if (configLoader.loadGatewayProfile(name.c_str())) {
                Serial.println("Profile loaded, reconnecting...");
                // Trigger reconnect
            } else {
                Serial.println("Profile not found");
            }
        }
        else if (cmd.startsWith("wifi ")) {
            // Format: wifi SSID PASSWORD
            int space = cmd.indexOf(' ', 5);
            if (space > 0) {
                String ssid = cmd.substring(5, space);
                String pass = cmd.substring(space + 1);
                configLoader.saveWifiToNvs(ssid, pass);
                Serial.println("Wi-Fi saved to NVS. Reboot to apply.");
            }
        }
        else if (cmd == "reload") {
            configLoader.loadConfig();
            Serial.println("Config reloaded");
        }
    }
}

*/

/*
 * FILESYSTEM UPLOAD COMMANDS
 * ==========================
 *
 * First time setup (uploads config files to device):
 *
 *   pio run -t uploadfs -e tlorapager_k257
 *
 * Or using the PlatformIO IDE: Click "Upload Filesystem Image"
 *
 * The data/ folder structure should be:
 *
 *   data/
 *   └── config/
 *       ├── tlora_terminal_config.xml
 *       ├── profiles/
 *       │   ├── lan.xml
 *       │   └── tailscale_funnel.xml
 *       ├── keymaps/
 *       │   └── us_qwerty.xml
 *       └── themes/
 *           └── nasa_minimal.xml
 *
 *
 * SETTING WI-FI CREDENTIALS
 * =========================
 *
 * Option A: Edit config/tlora_terminal_config.xml before upload
 *           (credentials in plaintext on filesystem)
 *
 * Option B: Use NVS (recommended - no plaintext file)
 *           After first boot, send via Serial:
 *             wifi YourSSID YourPassword
 *
 * Option C: Use the NvsSetup sketch (see NvsSetup.ino)
 *
 */

#endif // CONFIG_LOADER_INTEGRATION_H
