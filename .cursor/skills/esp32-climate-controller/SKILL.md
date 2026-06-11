---
name: esp32-climate-controller
description: >-
  Expert guidance for Arduino Nano ESP32 climate controllers — sensor integration,
  PID control, OLED/TFT UI, web dashboards, Wi-Fi/MQTT, persistent storage, power
  optimization, actuator safety, and modular architecture. Use when writing or
  debugging .ino/.cpp/.h files for grow boxes, HVAC controllers, environmental
  automation, BME280/DHT22/SCD40/SHT3x sensors, GyverPID/PID_v1, U8g2/SSD1306
  displays, ESPAsyncWebServer, PubSubClient, Preferences.h, or Home Assistant
  integration on ESP32.
---

# ESP32 Climate Controller

Expert embedded development for **Arduino Nano ESP32** climate controllers using the **Arduino IDE** (not PlatformIO).

## Mandatory workflow

Before implementing anything:

1. Read the main `.ino` and trace `#include` to identify **installed libraries**.
2. Scan existing modules in `firmware/Psi_Unit_Firmware/` (`config.h`, `sensors.*`, `logic.*`, `storage.*`, etc.) for patterns already in use.
3. Follow the **library priority** below — never reimplement what a library already provides.
4. After changes, remind the user to compile and upload in Arduino IDE.

## Library priority

| Priority | Action |
|----------|--------|
| 1 | Use an already-installed / already-included library |
| 2 | Use Arduino / ESP32 core features (`Preferences`, `Wire`, deep sleep, WDT) |
| 3 | Only as last resort — write custom code |

**Forbidden:** custom PID when GyverPID or PID_v1 is present; custom display drivers when U8g2 / Adafruit_SSD1306 / TFT_eSPI is present; duplicating library functionality.

## Core expertise

- **Sensor Integration**: Expert in reading, calibrating and filtering environmental sensors (BME280, DHT22, SCD40, SHT3x, etc.). Includes proper I2C error handling, data averaging, median filtering, and compensation techniques.
- **PID Control**: Professional PID implementation and tuning. Must use existing libraries (GyverPID preferred, or PID_v1). Includes anti-windup, output limiting, and parameter auto-tuning methods.
- **Display & User Interface**: Creating clear, responsive menus, status screens, real-time graphs and navigation on OLED/TFT displays (Adafruit_SSD1306, U8g2, TFT_eSPI).
- **Web Dashboard**: Building modern, responsive web interfaces with ESPAsyncWebServer, including real-time data updates (WebSocket or SSE), parameter configuration, and charts.
- **Connectivity**: Reliable Wi-Fi connection management, MQTT integration (PubSubClient or AsyncMqttClient), and Home Assistant discovery.
- **Persistent Storage**: Using Preferences.h (ESP32) and LittleFS for saving all settings, calibration values, and logs.
- **Power Optimization**: Deep Sleep modes, timed wake-ups, Wi-Fi power saving, and overall low-power design.
- **Safety & Reliability**: Critical safety systems for actuators (heaters, humidifiers, fans) — timeouts, failsafes, overheat protection, watchdog timer usage.
- **Code Architecture**: Clean modular design with config.h, separate modules (.cpp/.h), finite state machines, and good memory management.
- **Debugging & Diagnostics**: Advanced Serial logging, error tracing, memory usage optimization (RAM and Flash), and performance analysis.
- **Best Practices**: Always prefer using installed libraries, follow Arduino/ESP32 idioms, write stable and production-ready code.

## Project architecture (this repo)

Match existing layout:

```
firmware/Psi_Unit_Firmware/Psi_Unit_Firmware.ino → thin entry (setup/loop delegates)
firmware/Psi_Unit_Firmware/config.h            → pins, constants, valid ranges, I2C addresses
app_setup.cpp / app_loop.cpp → init and main loop orchestration
sensors.*                  → read, validate, filter, error flags
logic.*                    → actuator control, boot hold, timeouts
storage.*                  → Preferences (NVS), CRC, debounced save
display_ui.* / ui_input.*  → menus, encoder navigation
i2c_bus.*                  → shared Wire init and recovery
types.h                    → shared structs and enums
```

**Conventions in this project:**
- Pins and limits live in `firmware/Psi_Unit_Firmware/config.h` — never hardcode in modules.
- Sensor values as fixed-point (`int16_t` deci-degrees / percent) where applicable.
- Per-sensor `SensorState` with validation, EMA smoothing, and display delta thresholds.
- Outputs: boot hold (`OUTPUT_BOOT_HOLD_MS`), force-all-off on fault, HIGH-trigger relays.
- Storage: load on boot, debounced save (`storage_requestSave` / `storage_pollSave`).

## Domain checklists

### Sensors

```
- [ ] I2C init once; probe address; handle NACK / bus lockup (recovery reset)
- [ ] Validate range before use; set error flags; invalidate stale data on failure
- [ ] Filter: EMA, median (GyverFilters / RunningMedian), or library compensation
- [ ] Non-blocking reads where possible (e.g. DS18B20 conversion timing)
- [ ] DEBUG_SERIAL-gated probe logs
```

### PID & control

```
- [ ] GyverPID or PID_v1 — set sample time, output limits, anti-windup
- [ ] Hysteresis or minimum cycle time for relays (avoid chatter)
- [ ] Separate setpoints per mode; all tunables exposed via UI / web / storage
- [ ] On sensor fault: safe output state (off or last-known-safe policy)
```

### Actuator safety

```
- [ ] Boot hold: all outputs OFF until delay elapsed
- [ ] Max-on timeouts for heat, humidifier, INC
- [ ] Over-temp / over-humidity cutoffs
- [ ] esp_task_wdt or hardware WDT fed in loop
- [ ] Force outputs off on critical sensor loss
```

### Display UI

```
- [ ] U8g2 buffer mode appropriate for RAM (1-page vs full buffer)
- [ ] Partial redraw only when value delta exceeds threshold
- [ ] Encoder: debounce, acceleration, bounded menus
- [ ] Status screen: errors visible; sysinfo for diagnostics
```

### Web / MQTT (when added)

```
- [ ] ESPAsyncWebServer + AsyncTCP; avoid blocking loop
- [ ] WebSocket or SSE for live telemetry
- [ ] REST or form endpoints mirror storage fields
- [ ] MQTT: LWT, reconnect backoff, retained config topics
- [ ] Home Assistant MQTT discovery JSON where applicable
```

### Storage

```
- [ ] Preferences namespace; struct + CRC16 integrity check
- [ ] Defaults in storage_loadDefaults(); validate ranges after load
- [ ] Debounce writes; log save/load failures
- [ ] LittleFS only for logs, assets, or web static files — not hot settings
```

### Power

```
- [ ] esp_sleep_enable_* only when features allow (RTC wake, GPIO wake)
- [ ] WiFi.setSleep(true) when latency tolerable
- [ ] Reduce poll rates and display refresh in idle modes
```

### Debugging

```
- [ ] DEBUG_SERIAL macro gates verbose logs
- [ ] Log free heap periodically in sysinfo when diagnosing
- [ ] Tag logs by module: [SENS], [LOGIC], [STOR], [I2C]
```

## Response format

When proposing changes:

1. **Analysis** — which installed libraries apply and how.
2. **Solution** — minimal diff aligned with existing modules.
3. **Library** — name any new library and justify if none exists in project.
4. **Verify** — remind to compile in Arduino IDE; ask for Serial log if debugging.

Respond in **Russian** if the user writes in Russian.

## Additional resources

- Library defaults, code snippets, and tuning notes: [reference.md](reference.md)
