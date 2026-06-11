# Psi_Unit Firmware

> **Languages:** English | [Русский](docs/ru/README.md)

> ⚠️ **Disclaimer.** Firmware and documentation are an **amateur project** with no warranties. Mains (**~230 V AC**) wiring is **at your own risk**. The author is **not liable** for injury, fire, equipment damage, or other harm from building, flashing, or operating the device.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Dual-zone climate controller on **Arduino Nano ESP32**: sensors, OLED menu, SSR/relay outputs, NVS settings, optional Wi‑Fi SoftAP and web UI.

<img src="images/Psi_Unit_Device_Preview.jpg" height="400">

**Version:** `v0.1.0` (`FIRMWARE_VERSION` in `firmware/Psi_Unit_Firmware/config.h`)

## Repository layout

| Path | Contents |
|------|----------|
| **[firmware/Psi_Unit_Firmware/](firmware/Psi_Unit_Firmware/)** | Arduino sketch and all source files — open `.ino` in Arduino IDE |
| [docs/](docs/) | Wiring diagrams |
| [images/](images/) | Photos for README and BOM |
| [sim/](sim/) | Desktop UI simulator (SDL2) |
| [licenses/](licenses/) | Third-party license texts |

## Documentation

| File | Contents |
|------|----------|
| **[USER_GUIDE.md](USER_GUIDE.md)** | Operation — OLED menu, Wi‑Fi, safety, errors |
| [USER_GUIDE (RU)](docs/ru/USER_GUIDE.md) | User guide in Russian |
| [HARDWARE.md](HARDWARE.md) | Wiring, pins, sensors, libraries, flashing |
| [BOM.md](BOM.md) | Bill of materials |
| [BOM (RU)](docs/ru/BOM.md) | Bill of materials in Russian |
| [TEST_CHECKLIST.md](TEST_CHECKLIST.md) | Post-flash verification |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Firmware structure |
| [sim/README.md](sim/README.md) | Desktop UI simulator (SDL2) |

## Features

- **Sensors:** FS400-SHT30 (T/RH), DS18B20 (second zone), DS3231 (RTC)
- **Outputs:** GRH and INC heating, humidifier, fan (AUTO/MAN/ON/OFF), light (PWM + schedule)
- **UI:** SH1106 128×64, encoder, RGB status, splash, SYS screen
- **Settings:** dual-bank NVS (`SETTINGS_VER` **25**)
- **Serial CLI** (115200): `help`, `dump …`
- **Wi‑Fi SoftAP** + web (`http://192.168.4.1`): off until **SET → WiFi → Wi‑Fi SoftAP → ON**
- **Sensor log** in flash + web charts (`SENSOR_LOG_ENABLE`)
- **Output safety:** 3 s boot hold, critical fault, failsafe, max-on, over-temp — [USER_GUIDE](USER_GUIDE.md#errors-and-indication)
- **UI simulator** (SDL2, Windows): [sim/README.md](sim/README.md)

No MQTT or home Wi‑Fi (STA) — SoftAP only.

## Quick start

1. **Arduino IDE** → esp32 core **3.0.7**, board **Arduino Nano ESP32**
2. Install libraries from [HARDWARE.md](HARDWARE.md#required-libraries)
3. Open **`firmware/Psi_Unit_Firmware/Psi_Unit_Firmware.ino`** → **Upload**
4. If the old firmware still runs: **Tools → Erase All Flash Before Sketch Upload → Enabled**, upload again, **RST**, Serial **115200** — check `FIRMWARE_VERSION`, `Build:`, `Features:` ([TEST_CHECKLIST](TEST_CHECKLIST.md))
5. Operation — [USER_GUIDE.md](USER_GUIDE.md)

## Release

1. Update `FIRMWARE_VERSION_*` in `firmware/Psi_Unit_Firmware/config.h` (and `-sim` for the simulator).
2. Update the version line in this README.
3. Commit `Release vX.Y.Z`, tag `git tag -a vX.Y.Z`, push tag.
4. Build in Arduino IDE → [TEST_CHECKLIST](TEST_CHECKLIST.md).

**Sensor vs library:** device uses **FS400-SHT30** (chip **SHT30**); install **Adafruit SHT31 Library** (I²C **0x44**).

Licenses: [licenses/](licenses/). Firmware: [LICENSE](LICENSE).

## Serial CLI

Serial Monitor **115200**, line ending **Newline**. See [TEST_CHECKLIST](TEST_CHECKLIST.md) §9.

| Command | Purpose |
|---------|---------|
| `help` | Command list |
| `dump` / `dump all` | Full snapshot |
| `dump settings` | NVS setpoints |
| `dump outputs` | Relay/SSR state |
| `dump ui` | Current screen |
| `dump wifi` | SoftAP state |
| `wifi start` / `wifi stop` | Enable / disable SoftAP |

Disable: `#define SERIAL_CLI_ENABLE 0` in `firmware/Psi_Unit_Firmware/config.h`.
