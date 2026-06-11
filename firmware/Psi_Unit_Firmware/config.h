/*
 * config.h — firmware-wide configuration
 *
 * Hardware pin assignments, firmware version, retry timeouts, menu setpoint limits,
 * and compile-time flags for Wi-Fi, sensor log, and debug Serial.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Desktop simulator (sim/) uses a different constant declaration syntax.
#if defined(SIM_BUILD)
#define PSI_CONFIG_DECL inline constexpr
#else
#define PSI_CONFIG_DECL const
#endif

// D2–D11 pin aliases when the Arduino core does not define them.
#ifndef D2
#define D2 2
#define D3 3
#define D4 4
#define D5 5
#define D6 6
#define D7 7
#define D8 8
#define D9 9
#define D11 11
#endif
// --- Encoder, sensors, relays, and I2C pins (see HARDWARE.md) ---
#define ENC_A D4
#define ENC_B D3
#define ENC_BTN D5
// DS18B20 on A6 (see HARDWARE.md). Do not share the I2C bus line.
#define DS18_PIN A6
#define RELAY_FAN D8
#define RELAY_LIGHT D9
#define SSR_INC D2
#define SSR_HEAT D6
#define SSR_HUM D7
#define I2C_SDA A4
#define I2C_SCL A5

// Firmware SemVer version string.
#define FIRMWARE_VERSION_MAJOR 0
#define FIRMWARE_VERSION_MINOR 1
#define FIRMWARE_VERSION_PATCH 0
#if defined(SIM_BUILD)
#define FIRMWARE_VERSION "v0.1.0-sim"
#else
#define FIRMWARE_VERSION "v0.1.0"
#endif

// 1 = Wi-Fi SoftAP and web UI. 0 = module excluded at compile time.
#ifndef WIFI_ENABLE
#define WIFI_ENABLE 1
#endif

// 1 = sensor history on flash and browser charts. Usually equals WIFI_ENABLE.
#ifndef SENSOR_LOG_ENABLE
#define SENSOR_LOG_ENABLE WIFI_ENABLE
#endif

#if SENSOR_LOG_ENABLE
#define SENSOR_LOG_INTERVAL_MIN 5
#define SENSOR_LOG_INTERVAL_MAX 15
#define SENSOR_LOG_RETENTION_DAYS 30
// Ring buffer capacity (~30 days at a 5-minute interval).
#define SENSOR_LOG_CAPACITY ((SENSOR_LOG_RETENTION_DAYS * 24U * 60U) / SENSOR_LOG_INTERVAL_MIN)
// Web history JSON buffer and max chart points (RAM budget on ESP32).
#define SENSOR_LOG_HISTORY_JSON_CAP 8192
#define SENSOR_LOG_HISTORY_MAX_POINTS 200
#define SENSOR_LOG_HISTORY_DEFAULT_POINTS 150
// Requires a data flash partition (LittleFS / FFat in the Arduino partition scheme).
#endif

#define WIFI_AP_PASS_LEN 8

#if WIFI_ENABLE
// Network name: PsiUnit-A3F2 (suffix is unique per board)
#define WIFI_AP_SSID_PREFIX "PsiUnit-"
#define WIFI_AP_IP "192.168.4.1"
#define WIFI_AP_HOSTNAME "psiunit"
#define WIFI_HTTP_PORT 80
#define WIFI_AP_MAX_CLIENTS 4
#define WIFI_AP_CHANNEL 6
// 1 = start SoftAP immediately after power-on (without menu action)
#ifndef WIFI_AP_AUTO_START
#define WIFI_AP_AUTO_START 0
#endif
// Auto-stop SoftAP after N minutes (0 = until user turns it off)
#define WIFI_AP_SESSION_MIN_DEFAULT 30
#define WIFI_AP_SESSION_NEVER 0
// Allowed SoftAP session timeouts (min): Never (0), 30, 60, 120, 240
#define WIFI_AP_SESSION_MIN_MAX 240
#define WIFI_AP_SESSION_CHOICE_COUNT 5
#define WIFI_QR_TEXT_VISIBLE 5
#define WIFI_SESSION_MIN_MS 60000UL
// 0 = /api/* access for clients already connected to SoftAP (no browser prompt)
#ifndef WIFI_WEB_AUTH_ENABLE
#define WIFI_WEB_AUTH_ENABLE 0
#endif
#ifndef WIFI_WEB_SESSION_MS
#define WIFI_WEB_SESSION_MS 3600000UL
#endif
#endif

// Firmware capability string (Serial Features: and dump system)
#if WIFI_ENABLE && SENSOR_LOG_ENABLE
#define FIRMWARE_FEATURES "snapshot,commands,display_backend,input_backend,serial_cli,wifi_ap,web_sys,web_status,web_settings,web_cmd,wifi_ui,sensor_log"
#elif WIFI_ENABLE
#define FIRMWARE_FEATURES "snapshot,commands,display_backend,input_backend,serial_cli,wifi_ap,web_sys,web_status,web_settings,web_cmd,wifi_ui"
#else
#define FIRMWARE_FEATURES "snapshot,commands,display_backend,input_backend,serial_cli,wifi_off"
#endif

// 1 = verbose Serial messages at boot and during I2C/DS18 polling
#ifndef DEBUG_SERIAL
#define DEBUG_SERIAL 0
#endif

// 1 = USB help/dump commands for service diagnostics
#ifndef SERIAL_CLI_ENABLE
#define SERIAL_CLI_ENABLE 1
#endif

// 1 = automatic EU daylight saving time for DS3231
#ifndef RTC_DST_EU
#define RTC_DST_EU 1
#endif

// --- Sys Info screen, UI intervals, and output safety ---
PSI_CONFIG_DECL unsigned long SYSINFO_UPDATE_INTERVAL_MS = 1000UL;
PSI_CONFIG_DECL uint8_t SYSINFO_VISIBLE_LINES = 5;
PSI_CONFIG_DECL uint8_t SYSINFO_LABEL_X = 4;
PSI_CONFIG_DECL uint8_t SYSINFO_VALUE_RIGHT = 120;
PSI_CONFIG_DECL uint8_t SYSINFO_CLIP_RIGHT = 122;

PSI_CONFIG_DECL unsigned long SERIAL_BOOT_WAIT_MS = 500UL;
PSI_CONFIG_DECL int16_t DISPLAY_TEMP_DELTA = 1;   // display step 0.1 °C
PSI_CONFIG_DECL int16_t DISPLAY_HUM_DELTA = 1;    // display step 1 %
const uint32_t LIGHT_PWM_FREQ_HZ = 250;
const uint8_t LIGHT_PWM_RES_BITS = 8;
PSI_CONFIG_DECL unsigned long STORAGE_SAVE_DEBOUNCE_MS = 500UL;
// After power-on/reset all SSRs/relays stay off until this elapses (safety).
PSI_CONFIG_DECL unsigned long OUTPUT_BOOT_HOLD_MS = 3000UL;
// Emergency heat shutoff when t >= threshold (x10 °C). 0 = check disabled.
PSI_CONFIG_DECL int16_t TEMP_SAFETY_MAX = 350;
PSI_CONFIG_DECL int16_t INC_SAFETY_MAX = 350;
// Max continuous ON time for heat / INC SSR (min). 0 = no limit.
PSI_CONFIG_DECL uint16_t HEAT_MAX_ON_MIN = 240;
PSI_CONFIG_DECL uint16_t INC_MAX_ON_MIN = 240;
// Auto-clear Light OVR (long press on main screen). 0 = no limit.
PSI_CONFIG_DECL unsigned long LIGHT_OVERRIDE_MAX_MIN = 120UL;

// Serial warning on low heap (ESP32). 0 = disable check.
#ifndef HEAP_WARN_BYTES
#define HEAP_WARN_BYTES 20480UL
#endif
#ifndef HEAP_POLL_INTERVAL_MS
#define HEAP_POLL_INTERVAL_MS 30000UL
#endif

// I2C addresses
PSI_CONFIG_DECL uint8_t SHT30_I2C_ADDR = 0x44;
PSI_CONFIG_DECL uint8_t OLED_I2C_ADDR = 0x3C;
PSI_CONFIG_DECL uint8_t RTC_I2C_ADDR = 0x68;

// Timers, UI, sensors
PSI_CONFIG_DECL uint16_t SETTINGS_SIG = 0xA5C4;
PSI_CONFIG_DECL uint8_t SETTINGS_VER = 25;
PSI_CONFIG_DECL uint8_t SMOOTH_ALPHA = 35;
PSI_CONFIG_DECL uint8_t SMOOTH_BETA = 65;
PSI_CONFIG_DECL unsigned long READ_INTERVAL = 2000UL;
PSI_CONFIG_DECL unsigned long DISPLAY_INTERVAL = 50UL;
// DS18B20 conversion time (10-bit) plus margin
PSI_CONFIG_DECL unsigned long DS18_CONV_TIME = 250UL;
PSI_CONFIG_DECL unsigned long BTN_DEBOUNCE = 50UL;
PSI_CONFIG_DECL unsigned long LONG_PRESS_TIME = 1000UL;
PSI_CONFIG_DECL unsigned long EDIT_INACTIVITY_TIMEOUT = 300000UL;
PSI_CONFIG_DECL uint8_t MENU_VISIBLE_LINES = 5;
PSI_CONFIG_DECL unsigned long UI_TOAST_DURATION_MS = 2000UL;
PSI_CONFIG_DECL uint8_t ENC_TICKS_PER_STEP = 4;
PSI_CONFIG_DECL unsigned long ENC_READ_INTERVAL = 10UL;
#if defined(SIM_BUILD)
PSI_CONFIG_DECL unsigned long SPLASH_DURATION_MS = 1200UL;
#else
PSI_CONFIG_DECL unsigned long SPLASH_DURATION_MS = 4000UL;
#endif
PSI_CONFIG_DECL unsigned long I2C_HEALTH_INTERVAL_MS = 5000UL;

// SHT30 / DS18 reconnect backoff on error (ms): 2 -> 5 -> 10 -> 30 s
PSI_CONFIG_DECL unsigned long SENSOR_RECONNECT_BACKOFF_MS[] = {2000UL, 5000UL, 10000UL, 30000UL};
PSI_CONFIG_DECL uint8_t SENSOR_RECONNECT_BACKOFF_STEPS =
    sizeof(SENSOR_RECONNECT_BACKOFF_MS) / sizeof(SENSOR_RECONNECT_BACKOFF_MS[0]);

inline bool millisDeadlineReached(unsigned long nowMs, unsigned long deadlineMs) {
  return (long)(nowMs - deadlineMs) >= 0;
}

inline bool millisIntervalElapsed(unsigned long nowMs, unsigned long sinceMs, unsigned long intervalMs) {
  return (nowMs - sinceMs) >= intervalMs;
}

inline unsigned long sensorReconnectBackoffMs(uint8_t step) {
  if (step >= SENSOR_RECONNECT_BACKOFF_STEPS) {
    step = SENSOR_RECONNECT_BACKOFF_STEPS - 1;
  }
  return SENSOR_RECONNECT_BACKOFF_MS[step];
}

// Allowed ranges (temperature x10 in int16)
PSI_CONFIG_DECL int16_t SENSOR_INVALID = (int16_t)-32768;

PSI_CONFIG_DECL int16_t TEMP_MIN = -400;
PSI_CONFIG_DECL int16_t TEMP_MAX = 850;
PSI_CONFIG_DECL int16_t TEMP_SET_MIN = 220;
PSI_CONFIG_DECL int16_t TEMP_SET_MAX = 300;
// Temperature hysteresis (x10 °C): 0.1..5.0 °C
PSI_CONFIG_DECL uint8_t TEMP_HYST_MIN = 1;
PSI_CONFIG_DECL uint8_t TEMP_HYST_MAX = 50;
PSI_CONFIG_DECL int16_t HUM_MIN = 0;
PSI_CONFIG_DECL int16_t HUM_MAX = 100;
PSI_CONFIG_DECL int16_t HUM_SET_MIN = 50;
PSI_CONFIG_DECL int16_t HUM_SET_MAX = 100;

PSI_CONFIG_DECL unsigned long SENSOR_TIMEOUT = 10000UL;
PSI_CONFIG_DECL uint8_t SENSOR_ERROR_THRESHOLD = 3;

#endif // CONFIG_H
