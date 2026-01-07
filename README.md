# LilyGo T-LoRa Pager Terminal Client

A WebSocket terminal client for the [LilyGo T-LoRa Pager](https://lilygo.cc/products/t-lora-pager) that connects to a ttyd gateway for remote shell access.

Uses **LilyGoLib** for hardware abstraction - the same approach proven in the [k257-sat-tracker](https://github.com/Averroeskw/k257-sat-tracker) project.

## Hardware Specifications

| Component | Specification |
|-----------|---------------|
| **Display** | 2.33" IPS LCD, 480x222 pixels, ST7796 driver |
| **MCU** | ESP32-S3 (dual-core 240MHz) |
| **Flash** | 16MB |
| **PSRAM** | 8MB |
| **Keyboard** | QWERTY via TCA8418 I2C controller |
| **Input** | Rotary encoder with push button |
| **Radio** | LoRa SX1262 (433/868/915/920MHz) |
| **Audio** | ES8311 codec (mic + speaker + 3.5mm jack) |
| **NFC** | ST25R3916 high-performance NFC |
| **GNSS** | u-blox MIA-M10Q (4 concurrent constellations) |
| **Other** | RTC, haptic motor (DRV2605), battery gauge |
| **Size** | 143 x 110 x 43 mm |

## Pin Configuration (Reference)

### Display (ST7796)
| Function | GPIO |
|----------|------|
| CS | 38 |
| DC | 37 |
| Backlight | 42 (16-level PWM) |
| MOSI | 34 |
| MISO | 33 |
| SCK | 35 |

### I2C Bus (Shared)
| Function | GPIO |
|----------|------|
| SDA | 3 |
| SCL | 2 |

### I2C Devices
| Device | Address | Function |
|--------|---------|----------|
| TCA8418 | 0x34 | Keyboard controller |
| XL9555 | 0x20 | I/O expander |
| ES8311 | 0x18 | Audio codec |
| BQ27220 | 0x55 | Battery gauge |
| BHI260AP | 0x28 | AI IMU sensor |
| PCF85063A | 0x51 | RTC |
| BQ25896 | 0x6B | Power manager |
| DRV2605 | 0x5A | Haptic driver |

### Keyboard
| Function | GPIO/Value |
|----------|------------|
| I2C Address | 0x34 |
| Interrupt | GPIO 6 |
| Backlight | GPIO 46 |

### Rotary Encoder
| Function | GPIO |
|----------|------|
| A | 40 |
| B | 41 |
| Button | 7 |

## Requirements

### Hardware
- LilyGo T-LoRa Pager
- USB-C cable for programming

### Software
- [PlatformIO](https://platformio.org/)
- Libraries (automatically installed):
  - LilyGoLib (hardware abstraction)
  - LV_Helper (LVGL display)
  - WebSockets (by Links2004)
  - ArduinoJson

## Installation

### Step 1: Configure Settings

Edit `tlorapager_terminal/tlorapager_terminal.ino`:

```cpp
// Wi-Fi credentials
const char* WIFI_SSID = "YourNetworkName";
const char* WIFI_PASSWORD = "YourPassword";

// Gateway settings
const char* GATEWAY_HOST = "jetson.local";
const uint16_t GATEWAY_PORT = 7681;
```

### Step 2: Build and Upload

```bash
cd t-lora-pager
pio run --target upload

# Monitor serial output
pio device monitor
```

**Note:** No manual TFT_eSPI configuration needed! LilyGoLib handles display setup internally.

## Configuration

### Gateway Settings

```cpp
// LAN access (no SSL)
const char* GATEWAY_HOST = "192.168.1.100";
const uint16_t GATEWAY_PORT = 7681;
const char* GATEWAY_PATH = "/ws";
const bool USE_SSL = false;

// Tailscale Funnel (SSL)
const char* GATEWAY_HOST = "your-machine.ts.net";
const uint16_t GATEWAY_PORT = 443;
const char* GATEWAY_PATH = "/term/ws";
const bool USE_SSL = true;
```

## Terminal Features

- **80x18 character grid** (largest of all supported devices!)
- **Full QWERTY keyboard** with TCA8418 controller via LilyGoLib
- **Rotary encoder** - press for Enter, rotate for scroll (future)
- **Status bar** showing WiFi, WebSocket, and modifier key states
- **Haptic feedback** on key presses and bell character
- **Auto-reconnect** on disconnect
- **LVGL-based UI** for smooth rendering

## Keyboard

The T-LoRa Pager has a full QWERTY keyboard handled by LilyGoLib:

```
┌──────────────────────────────────────────────────┐
│  Q   W   E   R   T   Y   U   I   O   P   ⌫      │
│   A   S   D   F   G   H   J   K   L   ↵         │
│  ⇧   Z   X   C   V   B   N   M   ,   .   ⇧     │
│  Fn  Ctrl  Alt     SPACE      Sym  ←   →       │
└──────────────────────────────────────────────────┘
             [Rotary Encoder]
```

### Modifier Keys

| Key | Function |
|-----|----------|
| **Shift** | Toggle uppercase (auto-releases after one key) |
| **Ctrl** | Send control characters (Ctrl+C, Ctrl+D, etc.) |
| **Fn** | Function key modifier |
| **Sym** | Symbol key modifier |
| **Encoder Press** | Send Enter |
| **Encoder Rotate** | Scroll (future feature) |

### Key Bindings

| Key | Function |
|-----|----------|
| Enter | Send newline |
| Backspace | Send delete (0x7F) |
| Ctrl+C | Send interrupt (0x03) |
| Ctrl+D | Send EOF (0x04) |
| Ctrl+Z | Suspend (0x1A) |

## Troubleshooting

### Display issues

LilyGoLib handles display initialization automatically. If you have issues:
1. Ensure you're using the latest LilyGoLib from GitHub
2. Check serial output for initialization errors
3. Verify the board is a genuine T-LoRa Pager

### Keyboard not responding

1. Check serial output for `instance.getKeyChar()` return values
2. LilyGoLib handles TCA8418 initialization automatically
3. If using wrong board, keyboard I2C address may differ

### WiFi won't connect

1. Use 2.4GHz network (ESP32 doesn't support 5GHz)
2. Check SSID/password are correct
3. Try using IP address instead of hostname

### WebSocket connection fails

1. Verify ttyd is running: `systemctl status ttyd`
2. Check firewall allows port 7681
3. Test in browser first: `http://jetson.local:7681`

## File Structure

```
t-lora-pager/
├── README.md                        # This file
├── platformio.ini                   # Build configuration (uses LilyGoLib)
├── User_Setup.h                     # TFT_eSPI reference (not needed with LilyGoLib)
└── tlorapager_terminal/
    └── tlorapager_terminal.ino      # Main sketch using LilyGoLib
```

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        T-LoRa Pager                            │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │                    LilyGoLib                              │  │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐ │  │
│  │  │ Display  │  │ Keyboard │  │  Haptic  │  │  Radio   │ │  │
│  │  │ (LVGL)   │  │(TCA8418) │  │(DRV2605) │  │(SX1262)  │ │  │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────┘ │  │
│  └──────────────────────────────────────────────────────────┘  │
│                              │                                  │
│                    ┌─────────▼─────────┐                       │
│                    │  Terminal Client  │                       │
│                    │   (WebSocket)     │                       │
│                    └─────────┬─────────┘                       │
└──────────────────────────────┼──────────────────────────────────┘
                               │
                     WiFi (WebSocket)
                               │
                    ┌──────────▼──────────┐
                    │    ttyd Gateway     │
                    │   (Jetson/Linux)    │
                    └─────────────────────┘
```

## Advanced Features (Future)

The T-LoRa Pager has many features that could be integrated:

- **LoRa Mesh**: Use LoRa for terminal relay when WiFi unavailable
- **NFC**: Quick pairing or authentication
- **Audio**: Text-to-speech for accessibility
- **Haptic**: Enhanced feedback patterns
- **GNSS**: Location-aware commands
- **AI IMU**: Gesture controls
- **Scrollback**: Use encoder rotation for scrollback buffer

## Resources

- [T-LoRa Pager Product Page](https://lilygo.cc/products/t-lora-pager)
- [LilyGoLib Documentation](https://github.com/Xinyuan-LilyGO/LilyGoLib)
- [LILYGO Wiki](https://wiki.lilygo.cc/get_started/en/LoRa_GPS/T-LoraPager/T-LoraPager.html)
- [Meshtastic Support](https://meshtastic.org/docs/hardware/devices/lilygo/tpager/)

## Sources

- Configuration derived from [k257-sat-tracker](https://github.com/Averroeskw/k257-sat-tracker) project
- [CNX Software - T-LoRa Pager Overview](https://www.cnx-software.com/2025/08/12/lilygo-t-lora-pager-is-an-esp32-s3-handheld-with-support-for-text-messaging-ai-motion-detection-and-nfc/)
- [Liliputing - T-LoRa Pager](https://liliputing.com/lilygo-t-lora-pager-is-an-esp32-powered-communications-device-with-lora-nfc-and-gps/)
