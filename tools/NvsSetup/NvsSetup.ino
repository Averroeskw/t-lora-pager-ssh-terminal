/*
 * NVS Setup Utility for T-LoRa Pager Terminal
 * ============================================
 *
 * Flash this sketch ONCE to set Wi-Fi credentials in NVS (Non-Volatile Storage).
 * Then reflash the main tlorapager_terminal sketch.
 *
 * NVS persists across reflashes, so your credentials stay safe.
 *
 * Usage:
 * 1. Flash this sketch to your T-LoRa Pager
 * 2. Open Serial Monitor at 115200 baud
 * 3. Follow the prompts to enter Wi-Fi credentials
 * 4. Flash the main terminal sketch
 */

#include <Preferences.h>
#include <WiFi.h>

Preferences prefs;
const char* NVS_NAMESPACE = "tlora_cfg";

void setup() {
    Serial.begin(115200);
    delay(2000);  // Wait for serial monitor

    Serial.println();
    Serial.println("========================================");
    Serial.println("  T-LoRa Pager NVS Setup Utility");
    Serial.println("========================================");
    Serial.println();

    if (!prefs.begin(NVS_NAMESPACE, false)) {
        Serial.println("ERROR: Failed to initialize NVS!");
        while (1) delay(1000);
    }

    // Show current values (if any)
    String currentSsid = prefs.getString("wifi_ssid", "");
    String currentProfile = prefs.getString("last_profile", "");

    Serial.println("Current NVS values:");
    Serial.printf("  Wi-Fi SSID: %s\n", currentSsid.length() > 0 ? currentSsid.c_str() : "(not set)");
    Serial.printf("  Wi-Fi Pass: %s\n", prefs.isKey("wifi_pass") ? "****" : "(not set)");
    Serial.printf("  Last Profile: %s\n", currentProfile.length() > 0 ? currentProfile.c_str() : "(not set)");
    Serial.println();

    printMenu();
}

void printMenu() {
    Serial.println("Commands:");
    Serial.println("  wifi SSID PASSWORD  - Set Wi-Fi credentials");
    Serial.println("  profile NAME        - Set default gateway profile");
    Serial.println("  show                - Show current values");
    Serial.println("  clear               - Clear all NVS data");
    Serial.println("  test                - Test Wi-Fi connection");
    Serial.println("  help                - Show this menu");
    Serial.println();
    Serial.print("> ");
}

void loop() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if (cmd.length() == 0) {
            Serial.print("> ");
            return;
        }

        Serial.println(cmd);  // Echo

        if (cmd.startsWith("wifi ")) {
            handleWifi(cmd.substring(5));
        }
        else if (cmd.startsWith("profile ")) {
            handleProfile(cmd.substring(8));
        }
        else if (cmd == "show") {
            handleShow();
        }
        else if (cmd == "clear") {
            handleClear();
        }
        else if (cmd == "test") {
            handleTest();
        }
        else if (cmd == "help") {
            printMenu();
        }
        else {
            Serial.println("Unknown command. Type 'help' for options.");
            Serial.print("> ");
        }
    }
}

void handleWifi(String args) {
    int space = args.indexOf(' ');
    if (space <= 0) {
        Serial.println("Usage: wifi SSID PASSWORD");
        Serial.println("  Example: wifi MyNetwork MySecretPass123");
        Serial.print("> ");
        return;
    }

    String ssid = args.substring(0, space);
    String pass = args.substring(space + 1);

    prefs.putString("wifi_ssid", ssid);
    prefs.putString("wifi_pass", pass);

    Serial.println();
    Serial.println("Wi-Fi credentials saved to NVS!");
    Serial.printf("  SSID: %s\n", ssid.c_str());
    Serial.printf("  Pass: %s\n", String(pass.length()) + " characters");
    Serial.println();
    Serial.println("You can now flash the main terminal sketch.");
    Serial.print("> ");
}

void handleProfile(String name) {
    name.trim();
    if (name.length() == 0) {
        Serial.println("Usage: profile NAME");
        Serial.println("  Example: profile lan");
        Serial.println("  Example: profile tailscale_funnel");
        Serial.print("> ");
        return;
    }

    prefs.putString("last_profile", name);
    Serial.printf("Default profile set to: %s\n", name.c_str());
    Serial.print("> ");
}

void handleShow() {
    Serial.println();
    Serial.println("NVS Contents:");
    Serial.println("-------------");

    String ssid = prefs.getString("wifi_ssid", "");
    String pass = prefs.getString("wifi_pass", "");
    String profile = prefs.getString("last_profile", "");

    Serial.printf("  wifi_ssid:    %s\n", ssid.length() > 0 ? ssid.c_str() : "(empty)");
    Serial.printf("  wifi_pass:    %s\n", pass.length() > 0 ? "(set, hidden)" : "(empty)");
    Serial.printf("  last_profile: %s\n", profile.length() > 0 ? profile.c_str() : "(empty)");
    Serial.println();
    Serial.print("> ");
}

void handleClear() {
    Serial.println("Are you sure? Type 'YES' to confirm:");
    Serial.print("> ");

    while (!Serial.available()) delay(10);
    String confirm = Serial.readStringUntil('\n');
    confirm.trim();
    Serial.println(confirm);

    if (confirm == "YES") {
        prefs.clear();
        Serial.println("NVS cleared!");
    } else {
        Serial.println("Cancelled.");
    }
    Serial.print("> ");
}

void handleTest() {
    String ssid = prefs.getString("wifi_ssid", "");
    String pass = prefs.getString("wifi_pass", "");

    if (ssid.length() == 0) {
        Serial.println("No Wi-Fi credentials set. Use 'wifi SSID PASSWORD' first.");
        Serial.print("> ");
        return;
    }

    Serial.printf("Connecting to %s...\n", ssid.c_str());

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("SUCCESS! Connected to Wi-Fi");
        Serial.printf("  IP Address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("  Signal: %d dBm\n", WiFi.RSSI());
    } else {
        Serial.println("FAILED to connect.");
        Serial.println("Check SSID and password, or try moving closer to router.");
    }

    WiFi.disconnect();
    Serial.println();
    Serial.print("> ");
}
