# Architecture — Psi Unit Firmware

Structure for **v0.1.0** (`firmware/Psi_Unit_Firmware/`): UI, domain logic, platform, optional Wi‑Fi and sensor log.

## Layers

```text
┌──────────────────────────────────────────────────┐
│  UI: display_ui, ui_input                        │
│  (main screen reads AppSnapshot)                 │
├──────────────────────────────────────────────────┤
│  app_commands (write) │ app_snapshot (read)        │
│  app_effects (side effects)                      │
├──────────────────────────────────────────────────┤
│  display_backend_u8g2 │ input_backend_hw         │
├──────────────────────────────────────────────────┤
│  logic, sensors, rtc_clock, storage              │
│  settings_field, settings_runtime, settings_access│
├──────────────────────────────────────────────────┤
│  wifi_manager + wifi_web + wifi_qr  (WIFI_ENABLE)│
│  sensor_log                         (SENSOR_LOG) │
├──────────────────────────────────────────────────┤
│  app_platform, i2c_bus, rgb_status, system_info│
│  serial_cli                                      │
└──────────────────────────────────────────────────┘
```

## Entry point

| File | Role |
|------|------|
| `firmware/Psi_Unit_Firmware/Psi_Unit_Firmware.ino` | `app_setup()` + `app_loop()` |
| `app_setup.cpp` | Boot: NVS → I2C → sensors → display → Wi‑Fi → log |
| `app_loop.cpp` | Poll → logic → render |

`app_loop()` order: watchdog/CLI/I2C → splash/sleep → sensors/RTC → input → logic → display → RGB/Wi‑Fi/log.

## Settings storage

| Module | NVS namespace | Contents |
|--------|---------------|----------|
| `storage.cpp` | `psi_cfg` | Climate setpoints (`SETTINGS_VER` 25), dual-bank + CRC |
| `wifi_manager.cpp` | `psi_wifi` | AP password, timeout, user-off flag |
| `sensor_log.cpp` | `psi_log` | Log enable/interval; records on flash |

Wi‑Fi and log menu fields use `settings_runtime` + `app_commands`, not `SavedSettings`.

## Wi‑Fi and web

- SoftAP `PsiUnit-XXXX`, `192.168.4.1:80`
- Off by default; menu shows live `wifi_manager_isActive()`
- Routes in `wifi_web.cpp`; read via `app_snapshot` / `system_info`; write via `app_commands`
- QR: OLED + `GET /api/wifi/qr.svg`
- JSON in static buffers (`wifi_json_buf`); overflow → `json_overflow` error
- Disable: `WIFI_ENABLE 0`

## Output safety

- Boot hold: all outputs LOW for `OUTPUT_BOOT_HOLD_MS`
- **Critical fault:** NVS fail or all three sensor errors → all outputs OFF
- Per-channel failsafe on sensor error; constants in `firmware/Psi_Unit_Firmware/config.h`

## Serial CLI

115200, `SERIAL_CLI_ENABLE` (default 1): `help`, `dump`, `dump settings|sensors|outputs|snapshot|ui|system`.

## Source modules

All firmware sources live in **`firmware/Psi_Unit_Firmware/`**.

| Group | Files |
|-------|-------|
| Config | `config.h`, `types.h` |
| App | `app_setup`, `app_loop`, `app_platform`, `app_state`, `app_snapshot`, `app_commands`, `app_effects` |
| UI | `display_ui`, `ui_input`, `display_backend*`, `input_backend*`, `splash.h` |
| Domain | `logic`, `sensors`, `rtc_clock`, `storage`, `settings_*` |
| Connectivity | `wifi_manager`, `wifi_web`, `wifi_json_buf`, `wifi_qr` |
| Log | `sensor_log` |
| Platform | `i2c_bus`, `rgb_status`, `system_info`, `serial_cli` |

## Desktop simulator

Build from `sim/` (CMake + SDL2): same UI stack, SDL/keyboard backends. No Wi‑Fi or flash log. See [sim/README.md](sim/README.md).
