/**
 * T-LoRa Pager Terminal Client
 * SSH terminal client
 * Uses LilyGoLib for hardware initialization
 *
 * Features:
 * - Settings menu (rotary button)
 * - Multiple WiFi networks
 * - Local/Remote SSH server configuration
 * - Theme selection
 * - Persistent storage (NVS)
 */

#include <LilyGoLib.h>
#include <LV_Helper.h>
#include <WiFi.h>
#include "libssh_esp32.h"
#include <libssh/libssh.h>
#include "settings.h"
#include "settings_ui.h"

// Display dimensions
#define DISP_W 480
#define DISP_H 222

// Terminal configuration
#define TERM_FONT &lv_font_montserrat_12
#define TERM_BUFFER_SIZE 4096

// Rotary encoder pins defined in pins_arduino.h:
// ROTARY_A (40), ROTARY_B (41), ROTARY_C (42 - button)

// LVGL objects (exported for settings_ui)
lv_obj_t *terminalScreen = NULL;
lv_obj_t *terminalTA = NULL;
lv_obj_t *statusBar = NULL;
lv_obj_t *termStatusLabel = NULL;

// Terminal state
static char termBuffer[TERM_BUFFER_SIZE];
static int termBufferPos = 0;

// SSH state
static ssh_session sshSession = NULL;
static ssh_channel sshChannel = NULL;
static bool sshConnected = false;
static bool sshConnecting = false;
static TaskHandle_t sshTaskHandle = NULL;

// Thread-safe ring buffer for SSH -> display
#define SSH_RX_BUFFER_SIZE 2048
static char sshRxBuffer[SSH_RX_BUFFER_SIZE];
static volatile int sshRxHead = 0;
static volatile int sshRxTail = 0;
static SemaphoreHandle_t sshRxMutex = NULL;

// Input buffer
static char inputBuffer[256];
static int inputPos = 0;

// Rotary encoder state
static volatile int rotaryCount = 0;
static volatile bool rotaryPressed = false;
static int lastRotaryA = HIGH;

// Long press detection
static unsigned long btnPressStart = 0;
static bool btnWasPressed = false;
#define LONG_PRESS_MS 500

// Reconnection state
static bool wasConnected = false;
static unsigned long lastReconnectAttempt = 0;

// Forward declarations
void setupTerminalUI();
void updateStatus(const char* status);
void terminalPrint(const char* text);
void terminalPrintChar(char c);
void processKeyboard();
void processRotary();
void connectToWiFi();
void connectToServer();
void sshTask(void *pvParameters);
void sshSendKey(char key);
void sshDisconnect();
void sshRxPut(const char *data, int len);
void sshRxDrain();
void applyTheme();
void showIntro();
void playStartupHaptic();

// Rotary encoder ISR (inverted direction)
void IRAM_ATTR rotaryISR() {
    int a = digitalRead(ROTARY_A);
    int b = digitalRead(ROTARY_B);

    if (a != lastRotaryA) {
        if (b != a) {
            rotaryCount--;  // Inverted
        } else {
            rotaryCount++;  // Inverted
        }
        lastRotaryA = a;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== T-LoRa Pager Terminal ===");
    Serial.println("Version 1.0.0");

    // Initialize all hardware via LilyGoLib
    Serial.println("Initializing hardware...");
    uint32_t result = instance.begin();
    Serial.printf("LilyGoLib init: 0x%08X\n", result);

    // Initialize settings from NVS
    Serial.println("Loading settings...");
    settingsInit();

    // Initialize LVGL
    beginLvglHelper(instance);

    // Apply saved brightness
    instance.setBrightness(settings.brightness);

    // Show cool intro animation
    showIntro();

    // Setup terminal UI
    setupTerminalUI();

    // Apply saved theme
    applyTheme();

    // Initialize keyboard
    if (instance.initKeyboard()) {
        Serial.println("Keyboard: OK");
    } else {
        Serial.println("Keyboard: FAILED");
    }

    // Setup rotary encoder (pins from pins_arduino.h)
    pinMode(ROTARY_A, INPUT_PULLUP);
    pinMode(ROTARY_B, INPUT_PULLUP);
    pinMode(ROTARY_C, INPUT_PULLUP);  // Button
    attachInterrupt(digitalPinToInterrupt(ROTARY_A), rotaryISR, CHANGE);

    // Initialize settings UI
    settingsUIInit();

    // Create SSH receive buffer mutex
    sshRxMutex = xSemaphoreCreateMutex();

    // Connect to WiFi
    terminalPrint("T-LoRa Pager Terminal v1.0\n");
    terminalPrint("Press rotary button for settings\n\n");
    connectToWiFi();
}

void loop() {
    // Handle LVGL
    lv_task_handler();

    // Rotary button with long-press detection
    bool btnState = digitalRead(ROTARY_C);

    if (btnState == LOW && !btnWasPressed) {
        // Button just pressed
        btnPressStart = millis();
        btnWasPressed = true;
    } else if (btnState == HIGH && btnWasPressed) {
        // Button released
        unsigned long pressDuration = millis() - btnPressStart;
        btnWasPressed = false;

        if (pressDuration >= LONG_PRESS_MS) {
            // Long press = go back / cancel
            if (settingsUIIsVisible()) {
                MenuState_t state = settingsUIGetState();
                if (state == MENU_WIFI_ADD || state == MENU_WIFI_EDIT) {
                    // Cancel WiFi input - handled specially
                    settingsUICancelInput();
                } else {
                    settingsUIHandleKey('q');  // Back in menus
                }
            }
        } else if (pressDuration > 30) {  // Debounce
            // Short press = click/select
            if (settingsUIIsVisible()) {
                settingsUIHandleRotary(0);  // Click in menu
            } else {
                settingsUIShow();  // Open settings
            }
        }
    }

    // Handle rotary rotation
    processRotary();

    // Update WiFi scan if in progress
    if (settingsUIIsVisible() && settingsUIGetState() == MENU_WIFI_SCAN) {
        settingsUIUpdateWiFiList();
    }

    // Only process terminal if not in settings
    if (!settingsUIIsVisible()) {
        // Drain SSH receive buffer to display (thread-safe)
        sshRxDrain();

        // Process keyboard input (sends to SSH)
        processKeyboard();

        // Update status with RSSI periodically
        static unsigned long lastStatusUpdate = 0;
        if (millis() - lastStatusUpdate > 5000) {
            lastStatusUpdate = millis();
            updateStatusWithRSSI();
        }
    } else {
        // Forward keyboard to settings
        char key = 0;
        if (instance.getKeyChar(&key) > 0 && key != 0) {
            settingsUIHandleKey(key);
        }
    }

    delay(5);
}

void processRotary() {
    if (rotaryCount != 0) {
        int direction = (rotaryCount > 0) ? 1 : -1;
        rotaryCount = 0;

        if (settingsUIIsVisible()) {
            settingsUIHandleRotary(direction);
        } else if (terminalTA) {
            // Scroll terminal up/down
            lv_coord_t scrollAmount = 40;  // Pixels per detent
            if (direction > 0) {
                // Scroll down (toward newer content)
                lv_obj_scroll_by(terminalTA, 0, -scrollAmount, LV_ANIM_ON);
            } else {
                // Scroll up (toward older content)
                lv_obj_scroll_by(terminalTA, 0, scrollAmount, LV_ANIM_ON);
            }
        }
    }
}

void setupTerminalUI() {
    // Create terminal screen
    terminalScreen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(terminalScreen, lv_color_black(), 0);

    // Status bar at top
    statusBar = lv_obj_create(terminalScreen);
    lv_obj_set_size(statusBar, DISP_W, 20);
    lv_obj_set_pos(statusBar, 0, 0);
    lv_obj_set_style_bg_color(statusBar, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(statusBar, 0, 0);
    lv_obj_set_style_pad_all(statusBar, 2, 0);
    lv_obj_remove_flag(statusBar, LV_OBJ_FLAG_SCROLLABLE);

    termStatusLabel = lv_label_create(statusBar);
    lv_label_set_text(termStatusLabel, "T-LoRa Terminal");
    lv_obj_set_style_text_color(termStatusLabel, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_text_font(termStatusLabel, &lv_font_montserrat_12, 0);
    lv_obj_align(termStatusLabel, LV_ALIGN_LEFT_MID, 5, 0);

    // Terminal text area
    terminalTA = lv_textarea_create(terminalScreen);
    lv_obj_set_size(terminalTA, DISP_W, DISP_H - 22);
    lv_obj_set_pos(terminalTA, 0, 21);
    lv_obj_set_style_bg_color(terminalTA, lv_color_black(), 0);
    lv_obj_set_style_text_color(terminalTA, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_text_font(terminalTA, TERM_FONT, 0);
    lv_obj_set_style_border_width(terminalTA, 0, 0);
    lv_obj_set_style_pad_all(terminalTA, 4, 0);

    lv_textarea_set_text(terminalTA, "");
    lv_textarea_set_cursor_click_pos(terminalTA, false);
    // Enable scrolling for terminal
    lv_obj_add_flag(terminalTA, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(terminalTA, LV_SCROLLBAR_MODE_AUTO);

    // Clear buffer
    memset(termBuffer, 0, sizeof(termBuffer));
    termBufferPos = 0;

    // Load terminal screen
    lv_scr_load(terminalScreen);
}

void applyTheme() {
    const ThemeColors_t *theme = getCurrentTheme();

    if (terminalTA) {
        lv_obj_set_style_bg_color(terminalTA, lv_color_hex(theme->background), 0);
        lv_obj_set_style_text_color(terminalTA, lv_color_hex(theme->foreground), 0);
    }

    if (terminalScreen) {
        lv_obj_set_style_bg_color(terminalScreen, lv_color_hex(theme->background), 0);
    }

    if (statusBar) {
        lv_obj_set_style_bg_color(statusBar, lv_color_hex(theme->statusBar), 0);
    }

    if (termStatusLabel) {
        lv_obj_set_style_text_color(termStatusLabel, lv_color_hex(theme->foreground), 0);
    }
}

void updateStatus(const char* status) {
    if (termStatusLabel) {
        char buf[64];
        snprintf(buf, sizeof(buf), "T-LoRa | %s", status);
        lv_label_set_text(termStatusLabel, buf);
    }
}

// Update status bar with WiFi RSSI
void updateStatusWithRSSI() {
    if (!termStatusLabel) return;

    char buf[64];
    if (WiFi.status() == WL_CONNECTED) {
        int rssi = WiFi.RSSI();
        const char* signal;
        if (rssi > -50) signal = "****";
        else if (rssi > -60) signal = "*** ";
        else if (rssi > -70) signal = "**  ";
        else signal = "*   ";

        if (sshConnected) {
            snprintf(buf, sizeof(buf), "%s [%s] SSH Connected", WiFi.SSID().c_str(), signal);
        } else if (sshConnecting) {
            snprintf(buf, sizeof(buf), "%s [%s] SSH Connecting...", WiFi.SSID().c_str(), signal);
        } else {
            snprintf(buf, sizeof(buf), "%s [%s] Disconnected", WiFi.SSID().c_str(), signal);
        }
    } else {
        snprintf(buf, sizeof(buf), "No WiFi - Press rotary for settings");
    }
    lv_label_set_text(termStatusLabel, buf);
}

void terminalPrint(const char* text) {
    if (!terminalTA) return;

    int len = strlen(text);
    for (int i = 0; i < len && termBufferPos < TERM_BUFFER_SIZE - 1; i++) {
        termBuffer[termBufferPos++] = text[i];
    }
    termBuffer[termBufferPos] = '\0';

    // Trim buffer if too long (keep last half)
    if (termBufferPos > TERM_BUFFER_SIZE - 256) {
        int keepFrom = termBufferPos / 2;
        memmove(termBuffer, termBuffer + keepFrom, termBufferPos - keepFrom + 1);
        termBufferPos -= keepFrom;
    }

    lv_textarea_set_text(terminalTA, termBuffer);
    lv_textarea_set_cursor_pos(terminalTA, LV_TEXTAREA_CURSOR_LAST);
}

void terminalPrintChar(char c) {
    char buf[2] = {c, '\0'};
    terminalPrint(buf);
}

void connectToWiFi() {
    updateStatus("Scanning WiFi...");

    // Try saved networks in order
    bool connected = false;

    for (int i = 0; i < settings.wifiNetworkCount && !connected; i++) {
        if (!settings.wifiNetworks[i].enabled) continue;

        char msg[64];
        snprintf(msg, sizeof(msg), "Trying: %s\n", settings.wifiNetworks[i].ssid);
        terminalPrint(msg);
        updateStatus(settings.wifiNetworks[i].ssid);

        WiFi.begin(settings.wifiNetworks[i].ssid, settings.wifiNetworks[i].password);

        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            connected = true;
            char buf[64];
            snprintf(buf, sizeof(buf), "Connected: %s\n", WiFi.localIP().toString().c_str());
            terminalPrint(buf);
            Serial.println(buf);
        }
    }

    if (connected) {
        char buf[32];
        snprintf(buf, sizeof(buf), "IP: %s", WiFi.localIP().toString().c_str());
        updateStatus(buf);
        connectToServer();
    } else {
        updateStatus("No WiFi");
        terminalPrint("WiFi connection failed!\n");
        terminalPrint("Press rotary button to configure.\n");
    }

    terminalPrint("> ");
}

// Put data into SSH receive buffer (called from SSH task)
void sshRxPut(const char *data, int len) {
    if (sshRxMutex == NULL) return;
    if (xSemaphoreTake(sshRxMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        for (int i = 0; i < len; i++) {
            int nextHead = (sshRxHead + 1) % SSH_RX_BUFFER_SIZE;
            if (nextHead != sshRxTail) {  // Buffer not full
                sshRxBuffer[sshRxHead] = data[i];
                sshRxHead = nextHead;
            }
        }
        xSemaphoreGive(sshRxMutex);
    }
}

// Drain SSH receive buffer to display (called from main loop)
void sshRxDrain() {
    if (sshRxMutex == NULL) return;
    if (xSemaphoreTake(sshRxMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        char buf[256];
        int count = 0;

        while (sshRxTail != sshRxHead && count < sizeof(buf) - 1) {
            buf[count++] = sshRxBuffer[sshRxTail];
            sshRxTail = (sshRxTail + 1) % SSH_RX_BUFFER_SIZE;
        }

        xSemaphoreGive(sshRxMutex);

        if (count > 0) {
            buf[count] = '\0';
            terminalPrint(buf);
        }
    }
}

// SSH connection task - runs in separate FreeRTOS task
void sshTask(void *pvParameters) {
    ServerConfig_t *server = (ServerConfig_t *)pvParameters;
    int rc;

    Serial.println("SSH Task started");
    sshConnecting = true;

    // Initialize libssh
    libssh_begin();

    // Create SSH session
    sshSession = ssh_new();
    if (sshSession == NULL) {
        Serial.println("SSH: Failed to create session");
        sshConnecting = false;
        vTaskDelete(NULL);
        return;
    }

    // Set SSH options
    ssh_options_set(sshSession, SSH_OPTIONS_HOST, server->host);
    ssh_options_set(sshSession, SSH_OPTIONS_PORT, &server->port);
    ssh_options_set(sshSession, SSH_OPTIONS_USER, server->username);

    // Set timeout (10 seconds)
    long timeout = 10;
    ssh_options_set(sshSession, SSH_OPTIONS_TIMEOUT, &timeout);

    // Disable strict host key checking for embedded device
    ssh_options_set(sshSession, SSH_OPTIONS_STRICTHOSTKEYCHECK, 0);

    Serial.printf("SSH: Connecting to %s@%s:%d\n", server->username, server->host, server->port);

    // Connect
    rc = ssh_connect(sshSession);
    if (rc != SSH_OK) {
        Serial.printf("SSH: Connection failed: %s\n", ssh_get_error(sshSession));
        ssh_free(sshSession);
        sshSession = NULL;
        sshConnecting = false;
        vTaskDelete(NULL);
        return;
    }

    Serial.println("SSH: Connected, authenticating...");

    // Authenticate with password
    rc = ssh_userauth_password(sshSession, NULL, server->password);
    if (rc != SSH_AUTH_SUCCESS) {
        Serial.printf("SSH: Auth failed: %s\n", ssh_get_error(sshSession));
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        sshSession = NULL;
        sshConnecting = false;
        vTaskDelete(NULL);
        return;
    }

    Serial.println("SSH: Authenticated, opening channel...");

    // Create channel
    sshChannel = ssh_channel_new(sshSession);
    if (sshChannel == NULL) {
        Serial.println("SSH: Failed to create channel");
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        sshSession = NULL;
        sshConnecting = false;
        vTaskDelete(NULL);
        return;
    }

    // Open session on channel
    rc = ssh_channel_open_session(sshChannel);
    if (rc != SSH_OK) {
        Serial.printf("SSH: Failed to open session: %s\n", ssh_get_error(sshSession));
        ssh_channel_free(sshChannel);
        sshChannel = NULL;
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        sshSession = NULL;
        sshConnecting = false;
        vTaskDelete(NULL);
        return;
    }

    // Request PTY (terminal size 80x20)
    rc = ssh_channel_request_pty_size(sshChannel, "xterm", 80, 20);
    if (rc != SSH_OK) {
        Serial.printf("SSH: Failed to request PTY: %s\n", ssh_get_error(sshSession));
        ssh_channel_close(sshChannel);
        ssh_channel_free(sshChannel);
        sshChannel = NULL;
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        sshSession = NULL;
        sshConnecting = false;
        vTaskDelete(NULL);
        return;
    }

    // Request shell
    rc = ssh_channel_request_shell(sshChannel);
    if (rc != SSH_OK) {
        Serial.printf("SSH: Failed to request shell: %s\n", ssh_get_error(sshSession));
        ssh_channel_close(sshChannel);
        ssh_channel_free(sshChannel);
        sshChannel = NULL;
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        sshSession = NULL;
        sshConnecting = false;
        vTaskDelete(NULL);
        return;
    }

    Serial.println("SSH: Shell ready!");
    sshConnecting = false;
    sshConnected = true;

    // Read loop
    char buffer[512];
    while (sshConnected && ssh_channel_is_open(sshChannel) && !ssh_channel_is_eof(sshChannel)) {
        int nbytes = ssh_channel_read_nonblocking(sshChannel, buffer, sizeof(buffer) - 1, 0);

        if (nbytes > 0) {
            // Use thread-safe ring buffer instead of direct terminalPrint
            sshRxPut(buffer, nbytes);
            Serial.printf("SSH RX: %d bytes\n", nbytes);
        } else if (nbytes < 0) {
            Serial.println("SSH: Read error");
            break;
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    Serial.println("SSH: Connection ended");
    sshConnected = false;

    // Cleanup
    if (sshChannel) {
        ssh_channel_send_eof(sshChannel);
        ssh_channel_close(sshChannel);
        ssh_channel_free(sshChannel);
        sshChannel = NULL;
    }
    if (sshSession) {
        ssh_disconnect(sshSession);
        ssh_free(sshSession);
        sshSession = NULL;
    }

    sshTaskHandle = NULL;
    vTaskDelete(NULL);
}

// Send a key to SSH channel
void sshSendKey(char key) {
    if (!sshConnected || sshChannel == NULL) return;

    int nbytes = ssh_channel_write(sshChannel, &key, 1);
    if (nbytes < 0) {
        Serial.println("SSH: Write error");
    }
}

// Send data to SSH channel
void sshSendData(const char *data, size_t len) {
    if (!sshConnected || sshChannel == NULL) return;

    int nbytes = ssh_channel_write(sshChannel, data, len);
    if (nbytes < 0) {
        Serial.println("SSH: Write error");
    }
}

// Disconnect SSH
void sshDisconnect() {
    sshConnected = false;
    // The task will clean up on next iteration
}

void connectToServer() {
    static ServerConfig_t *server = NULL;

    // Choose server based on preference
    if (settings.preferRemote && settings.remoteServer.enabled) {
        server = &settings.remoteServer;
        terminalPrint("Connecting to remote SSH server...\n");
    } else if (settings.localServer.enabled) {
        server = &settings.localServer;
        terminalPrint("Connecting to local SSH server...\n");
    } else if (settings.remoteServer.enabled) {
        server = &settings.remoteServer;
        terminalPrint("Connecting to remote SSH server...\n");
    }

    if (server == NULL || strlen(server->host) == 0) {
        terminalPrint("No server configured!\n");
        return;
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "SSH: %s@%s:%d\n", server->username, server->host, server->port);
    terminalPrint(msg);
    Serial.printf("SSH Connect: host=%s port=%d user=%s\n",
                  server->host, server->port, server->username);

    // Start SSH task with larger stack (SSH needs ~50KB)
    xTaskCreatePinnedToCore(
        sshTask,           // Task function
        "ssh_task",        // Name
        51200,             // Stack size (50KB for SSH)
        server,            // Parameter
        5,                 // Priority
        &sshTaskHandle,    // Task handle
        1                  // Core 1 (leave core 0 for LVGL)
    );

    terminalPrint("SSH connecting...\n");
}

void processKeyboard() {
    char key = 0;
    if (instance.getKeyChar(&key) <= 0) return;
    if (key == 0) return;

    Serial.printf("Key: 0x%02X '%c'\n", key, key);

    // For SSH, send keys directly - the remote shell handles everything
    if (sshConnected && sshChannel) {
        // Handle special keys
        if (key == '\r') {
            // Send newline
            sshSendKey('\n');
        } else if (key == '\b' || key == 127 || key == 8) {
            // Send DEL (0x7F) - works for most shells
            sshSendKey(0x7F);
            Serial.println("Backspace sent as DEL");
        } else {
            // Send the key directly
            sshSendKey(key);
        }
    } else {
        // Not connected - show local echo for feedback
        if (key == '\n' || key == '\r') {
            terminalPrint("\n> ");
        } else if (key == '\b' || key == 127 || key == 8) {
            if (termBufferPos > 0) {
                termBufferPos--;
                termBuffer[termBufferPos] = '\0';
                lv_textarea_set_text(terminalTA, termBuffer);
            }
        } else if (key >= 32 && key < 127) {
            terminalPrintChar(key);
        }
    }
}

// Check for long-press backspace from keyboard
void checkLongPressBackspace() {
    static unsigned long lastBackspaceTime = 0;
    static bool backspaceHeld = false;

    // Check if backspace is being held (key code 8 or 127)
    char key = 0;
    if (instance.getKeyChar(&key) > 0 && (key == '\b' || key == 127 || key == 8)) {
        if (!backspaceHeld) {
            backspaceHeld = true;
            lastBackspaceTime = millis();
        } else if (millis() - lastBackspaceTime > 100) {
            // Repeat backspace every 100ms when held
            if (sshConnected && sshChannel) {
                sshSendKey(0x7F);
            }
            lastBackspaceTime = millis();
        }
    } else {
        backspaceHeld = false;
    }
}


// Intro variables - static buffer instead of String
static lv_obj_t *introScreen = NULL;
static lv_obj_t *introLabel = NULL;
#define INTRO_BUF_SIZE 512
static char introBuffer[INTRO_BUF_SIZE];
static int introBufPos = 0;

// Haptic feedback
void hapticTick() {
    if (!settings.hapticEnabled) return;
    instance.drv.setWaveform(0, 10);
    instance.drv.setWaveform(1, 0);
    instance.drv.run();
}

void hapticClick() {
    if (!settings.hapticEnabled) return;
    instance.drv.setWaveform(0, 1);
    instance.drv.setWaveform(1, 0);
    instance.drv.run();
}

// Clean, professional 2-second intro
void showIntro() {
    // Create intro screen
    introScreen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(introScreen, lv_color_black(), 0);
    lv_scr_load(introScreen);
    lv_task_handler();

    // Single haptic pulse on start
    hapticClick();

    // Create centered logo box
    lv_obj_t *logoBox = lv_obj_create(introScreen);
    lv_obj_set_size(logoBox, 300, 120);
    lv_obj_center(logoBox);
    lv_obj_set_style_bg_color(logoBox, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_border_color(logoBox, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_border_width(logoBox, 2, 0);
    lv_obj_set_style_radius(logoBox, 8, 0);
    lv_obj_set_style_pad_all(logoBox, 10, 0);
    lv_obj_set_style_shadow_color(logoBox, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_shadow_width(logoBox, 25, 0);
    lv_obj_set_style_shadow_opa(logoBox, LV_OPA_60, 0);
    lv_obj_set_style_opa(logoBox, 0, 0);  // Start invisible
    lv_obj_remove_flag(logoBox, LV_OBJ_FLAG_SCROLLABLE);

    // Title - T-LORA
    lv_obj_t *titleLbl = lv_label_create(logoBox);
    lv_obj_set_style_text_font(titleLbl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(titleLbl, lv_color_hex(0x00FF00), 0);
    lv_label_set_text(titleLbl, "T-LORA");
    lv_obj_align(titleLbl, LV_ALIGN_TOP_MID, 0, 5);

    // Subtitle - PAGER
    lv_obj_t *subLbl = lv_label_create(logoBox);
    lv_obj_set_style_text_font(subLbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(subLbl, lv_color_hex(0x00AA00), 0);
    lv_label_set_text(subLbl, "PAGER");
    lv_obj_align(subLbl, LV_ALIGN_CENTER, 0, 5);

    // Version
    lv_obj_t *verLbl = lv_label_create(logoBox);
    lv_obj_set_style_text_font(verLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(verLbl, lv_color_hex(0x555555), 0);
    lv_label_set_text(verLbl, "Terminal v1.0");
    lv_obj_align(verLbl, LV_ALIGN_BOTTOM_MID, 0, -5);

    lv_task_handler();

    // Fade in
    for (int i = 0; i <= 255; i += 17) {
        lv_obj_set_style_opa(logoBox, i > 255 ? 255 : i, 0);
        lv_task_handler();
        delay(10);
    }
    lv_obj_set_style_opa(logoBox, 255, 0);
    lv_task_handler();

    // Hold
    delay(1000);

    // Fade out
    for (int i = 255; i >= 0; i -= 17) {
        lv_obj_set_style_opa(logoBox, i < 0 ? 0 : i, 0);
        lv_task_handler();
        delay(10);
    }

    hapticClick();

    // Clean up
    lv_obj_del(introScreen);
    introScreen = NULL;
}
