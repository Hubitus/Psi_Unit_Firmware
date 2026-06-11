# ESP32 Climate Controller — Reference

Detailed patterns and library defaults. Read when implementing a specific subsystem.

## Preferred libraries

| Domain | Preferred | Alternatives |
|--------|-----------|--------------|
| Temperature/humidity I2C | Adafruit_SHT31 (SHT30 chip, e.g. FS400-SHT30), Adafruit_BME280 | SHT3x, Sensirion SCD4x |
| OneWire temp | DallasTemperature + OneWire | — |
| PID | **GyverPID** | PID_v1 |
| Filters | GyverFilters, RunningMedian | EMA inline (as in `sensor_smoothEma`) |
| OLED | **U8g2** | Adafruit_SSD1306, SH1106 |
| TFT | TFT_eSPI | Adafruit_GFX + driver |
| Encoder | Encoder (Paul Stoffregen) | — |
| RTC | RTClib (DS3231) | — |
| Web | ESPAsyncWebServer + AsyncTCP | — |
| MQTT | AsyncMqttClient | PubSubClient |
| Settings | Preferences.h (NVS) | LittleFS for files |
| JSON (web/MQTT) | ArduinoJson | — |

## I2C error handling

```cpp
// Probe before use; recover bus on stuck SDA/SCL
if (!i2c_probeDevice(addr)) {
  sensor_updateState(state, false);
  return false;
}
// On repeated failures: Wire.end(); delay(10); Wire.begin(SDA, SCL);
```

- Set clock conservatively (100 kHz) if long cables or many devices.
- Never call blocking sensor reads inside display draw paths.

## Sensor filtering

**EMA** (already used in this project):

```cpp
int16_t sensor_smoothEma(int16_t current, int16_t newSample) {
  return (int16_t)((current * 7 + newSample) / 8);  // alpha ≈ 0.125
}
```

**Median** (spike rejection): use RunningMedian or GyverFilters before EMA.

**SCD40**: always use Sensirion library compensation (temp/humidity reference) when co-located with SHT/BME.

## GyverPID template

```cpp
#include <GyverPID.h>
GyverPID pid(Kp, Ki, Kd, true);  // true = reverse if needed

void controlTick() {
  pid.setpoint = settings.heatSetpoint;
  pid.input = sensorTemp / 10.0f;
  pid.outputLimits(0, 255);
  pid.compute();
  // Map pid.output to SSR PWM or relay with min cycle time
}
```

Requirements: fixed `dt` or call at constant interval; anti-windup via `outputLimits`; tune Kp first, then Ki, then Kd.

## Relay / SSR cycling

Avoid short cycles that damage compressors or SSRs:

```cpp
const unsigned long MIN_HEAT_CYCLE_MS = 30000;
const unsigned long MIN_HUM_CYCLE_MS  = 60000;
// Track lastTransitionMs; ignore requests inside min cycle unless SAFETY OFF
```

## Display (U8g2)

```cpp
// SH1106 128x64 I2C — match constructor to hardware rotation
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
```

- Use `firstPage()` / `nextPage()` loop for 1-page buffer.
- Only redraw numeric fields when `sensor_displayValueChanged()` returns true.
- Keep menu depth ≤ 3 levels; show units and limits in edit screens.

## Web dashboard skeleton

```cpp
#include <ESPAsyncWebServer.h>
AsyncWebServer server(80);

// Static files from LittleFS: server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
// SSE: AsyncEventSource for telemetry push every N ms from loop timer, not per-request block
// API: POST /api/settings with JSON body → validate → storage_requestSave()
```

Use `AsyncWebSocket` for bidirectional control; throttle broadcasts to 1–2 Hz unless alarm.

## MQTT / Home Assistant

```cpp
// Discovery (climate example topic):
// homeassistant/climate/<node_id>/config
// Payload: name, temp_state_topic, temp_command_topic, min_temp, max_temp, modes[]
// LWT: topic availability, payload "offline" / "online"
```

Reconnect with exponential backoff (1s → 2s → … → 60s cap). Publish retained config once.

## Preferences storage pattern

```cpp
Preferences prefs;
prefs.begin("climate", false);
// Store struct as blob + CRC16 (see storage_calculateCRC16 in this project)
prefs.putBytes("settings", &data, sizeof(data));
prefs.end();
```

Validate every field after load; clamp to `config.h` min/max; fall back to defaults on CRC mismatch.

## Deep sleep

```cpp
esp_sleep_enable_timer_wakeup(sleepUs);
esp_sleep_enable_ext0_wakeup(WAKE_GPIO, level);
esp_deep_sleep_start();
// Re-init everything in setup(); restore state from Preferences
```

Save pending settings before sleep. Disable WiFi cleanly if waking rarely.

## Watchdog (ESP32)

```cpp
#include <esp_task_wdt.h>
esp_task_wdt_init(30, true);
esp_task_wdt_add(NULL);
// In loop: esp_task_wdt_reset();
```

Separate hardware safety (output timeouts) from WDT (runaway loop).

## Memory tips

- Prefer `char[]` + `snprintf` over `String` in hot paths.
- Use `PROGMEM` for fonts/bitmaps (splash, icons).
- Check `ESP.getFreeHeap()` after adding web/MQTT stacks.
- U8g2 full buffer ≈ 1 KB; page buffer ≈ 128 bytes — choose consciously.

## Serial logging levels

| Level | When |
|-------|------|
| Boot | Firmware version, storage load result, I2C scan summary |
| Error | Sensor fault, save failure, output forced off |
| Debug (`DEBUG_SERIAL`) | Per-read values, PID terms, menu transitions |

## Auto-tuning (PID)

If implementing relay auto-tune (Ziegler–Nichols or Tyreus–Luyben):

1. Run at fixed setpoint with relay bang-bang; measure oscillation period Pu and amplitude.
2. Apply rules; start conservative (50% of calculated Ki/Kd).
3. Store results in Preferences; expose via web for re-tune without reflash.

Prefer manual tuning for production grow environments unless user explicitly requests auto-tune.
