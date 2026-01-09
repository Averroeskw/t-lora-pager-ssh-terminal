# Build & Configuration Guide

## Prerequisites

- [PlatformIO CLI](https://platformio.org/install/cli) or PlatformIO IDE extension for VS Code
- USB-C cable
- T-LoRa Pager K257 device

## Quick Start

```bash
# 1. Clone the repository
git clone https://github.com/Averroeskw/t-lora-pager-ssh-terminal.git
cd t-lora-pager-ssh-terminal

# 2. Edit Wi-Fi credentials (see options below)

# 3. Upload filesystem (config files)
pio run -t uploadfs -e tlorapager_k257

# 4. Build and upload firmware
pio run -t upload -e tlorapager_k257

# 5. Monitor serial output
pio device monitor
```

## Setting Wi-Fi Credentials

### Option A: NVS (Recommended - Secure)

Wi-Fi credentials stored in NVS persist across firmware updates and aren't stored in plaintext files.

**Method 1: Flash NvsSetup utility**
```bash
cd tools/NvsSetup
pio run -t upload
pio device monitor
# Type: wifi YourSSID YourPassword
# Then reflash main firmware
```

**Method 2: Serial command after boot**
```
wifi YourSSID YourPassword
```

### Option B: Edit XML Config

Edit `data/config/tlora_terminal_config.xml`:
```xml
<wifi>
  <ssid>YourNetworkName</ssid>
  <password>YourPassword</password>
</wifi>
```

Then upload filesystem: `pio run -t uploadfs`

## Gateway Profiles

Pre-configured profiles in `data/config/profiles/`:

| Profile | File | Use Case |
|---------|------|----------|
| LAN | `lan.xml` | Local ttyd server |
| Tailscale | `tailscale_funnel.xml` | Remote via Tailscale Funnel |

### Switching Profiles

Via Serial:
```
profile lan
profile tailscale_funnel
```

Or edit `tlora_terminal_config.xml` gateway section directly.

### Creating Custom Profiles

```xml
<?xml version="1.0" encoding="UTF-8"?>
<gatewayProfile name="my_server" version="1.0">
  <host>myserver.example.com</host>
  <port>443</port>
  <path>/term/ws</path>
  <useSsl>true</useSsl>
</gatewayProfile>
```

Save as `data/config/profiles/my_server.xml` and re-upload filesystem.

## PlatformIO Commands

| Command | Description |
|---------|-------------|
| `pio run` | Build firmware |
| `pio run -t upload` | Build & flash firmware |
| `pio run -t uploadfs` | Upload LittleFS filesystem |
| `pio device monitor` | Serial monitor (115200 baud) |
| `pio run -t clean` | Clean build artifacts |

### Environments

| Environment | Description |
|-------------|-------------|
| `tlorapager_k257` | Release build (default) |
| `tlorapager_k257_debug` | Debug build with verbose logging |
| `tlorapager_k257_ota` | OTA update support |

```bash
# Debug build
pio run -t upload -e tlorapager_k257_debug
```

## Partition Layout

| Partition | Size | Purpose |
|-----------|------|---------|
| NVS | 20KB | Wi-Fi credentials, preferences |
| app0 | 3MB | Main firmware |
| app1 | 3MB | OTA update slot |
| spiffs | 1.5MB | LittleFS config files |
| coredump | 64KB | Crash diagnostics |

## Filesystem Structure

```
data/
└── config/
    ├── tlora_terminal_config.xml   # Main configuration
    ├── profiles/
    │   ├── lan.xml                 # LAN gateway profile
    │   └── tailscale_funnel.xml    # Tailscale profile
    ├── keymaps/
    │   └── us_qwerty.xml           # Keyboard mapping
    └── themes/
        └── nasa_minimal.xml        # UI theme
```

## Serial Commands

After booting, these commands are available via Serial Monitor:

| Command | Description |
|---------|-------------|
| `config` | Print current configuration |
| `profiles` | List available gateway profiles |
| `profile NAME` | Load gateway profile |
| `wifi SSID PASS` | Save Wi-Fi to NVS |
| `reload` | Reload config from filesystem |

## Troubleshooting

### "LittleFS mount failed"
Upload filesystem first: `pio run -t uploadfs`

### "Wi-Fi not connecting"
1. Verify 2.4GHz network (ESP32 doesn't support 5GHz)
2. Check credentials: `pio device monitor` then type `config`
3. Try re-setting via NVS: `wifi YourSSID YourPassword`

### "WebSocket connection failed"
1. Verify gateway is running (ttyd)
2. Check host/port/path in config
3. For SSL: ensure valid certificate or use `useSsl: false` for LAN

### Build errors with TinyXML2
```bash
pio pkg update
pio run -t clean
pio run
```
