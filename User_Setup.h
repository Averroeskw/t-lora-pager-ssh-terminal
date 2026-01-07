// TFT_eSPI User Setup for LilyGo T-LoRa Pager
// =============================================
//
// NOTE: When using LilyGoLib (recommended), this file is NOT needed.
// LilyGoLib handles all display configuration internally.
//
// This file is provided for reference or if you want to use
// raw TFT_eSPI without LilyGoLib.
//
// Display: 2.33" IPS 480x222, ST7796 Driver
//
// To use with raw TFT_eSPI (not recommended):
//   cp User_Setup.h ~/.platformio/lib/TFT_eSPI/User_Setup.h

#define USER_SETUP_ID 251  // T-LoRa Pager custom setup

// ============================================================================
// Display Driver - ST7796 480x222
// ============================================================================

#define ST7796_DRIVER

// Display dimensions (portrait mode - rotated in software)
#define TFT_WIDTH  222
#define TFT_HEIGHT 480

// Color order
#define TFT_RGB_ORDER TFT_BGR

// ============================================================================
// Pin Definitions - T-LoRa Pager (from k257-sat-tracker)
// ============================================================================

// SPI pins
#define TFT_MOSI 34
#define TFT_MISO 33
#define TFT_SCLK 35

// Control pins
#define TFT_CS   38    // Chip select
#define TFT_DC   37    // Data/Command
#define TFT_RST  -1    // Reset (not connected, use software reset)
#define TFT_BL   42    // Backlight (16-level PWM control)

// ============================================================================
// I2C Configuration (for reference)
// ============================================================================
// SDA: GPIO 3
// SCL: GPIO 2

// ============================================================================
// Keyboard (TCA8418) Configuration (for reference)
// ============================================================================
// I2C Address: 0x34
// Interrupt: GPIO 6
// Backlight: GPIO 46

// ============================================================================
// Rotary Encoder (for reference)
// ============================================================================
// A: GPIO 40
// B: GPIO 41
// Button: GPIO 7

// ============================================================================
// SPI Settings
// ============================================================================

#define SPI_FREQUENCY       40000000   // 40 MHz
#define SPI_READ_FREQUENCY  16000000   // 16 MHz for reads
#define SPI_TOUCH_FREQUENCY  2500000   // Touch not used

// ============================================================================
// Font Support
// ============================================================================

#define LOAD_GLCD    // Font 1. Original Adafruit 8 pixel font
#define LOAD_FONT2   // Font 2. Small 16 pixel high font
#define LOAD_FONT4   // Font 4. Medium 26 pixel high font
#define LOAD_FONT6   // Font 6. Large 48 pixel font
#define LOAD_FONT7   // Font 7. 7 segment 48 pixel font
#define LOAD_FONT8   // Font 8. Large 75 pixel font
#define LOAD_GFXFF   // FreeFonts

#define SMOOTH_FONT

// ============================================================================
// Optional Features
// ============================================================================

// Transaction support for shared SPI bus
#define SUPPORT_TRANSACTIONS
