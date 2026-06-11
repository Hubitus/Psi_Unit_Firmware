# Hardware test checklist (Nano ESP32)

Parts — [BOM.md](BOM.md). Wiring — [HARDWARE.md](HARDWARE.md).

After flashing `firmware/Psi_Unit_Firmware/Psi_Unit_Firmware.ino`: Serial **115200**, press **RST**.

**Boot line:** `FIRMWARE_VERSION`, `Build:`, `Features:`, `=== Setup complete ===`.  
If missing — **Erase All Flash Before Sketch Upload**, reflash, reset.

## 1. I2C scan

Expected: `0x3C` or `0x3D` (OLED), `0x44` (SHT30), `0x68` (RTC). Settings in NVS, not on I2C.

With `DEBUG_SERIAL 1`: `I2C OLED/SHT30/RTC OK`. Bus recovery on repeated errors (~5 s interval).

## 2. OLED

- Splash ~4 s, then main screen
- Menu: **INC, GRH, HUM, FAN, LGT, SET, BACK**
- Long press on main: light OVR; auto-off per `LIGHT_OVERRIDE_MAX_MIN` (default 120 min)
- `NO NVS` on bottom if NVS failed

## 3. RTC

CR2032 on DS3231 — [USER_GUIDE](USER_GUIDE.md#ds3231-rtc-and-backup-battery-vbat). After cell swap: set time; power off 5–10 min — time must hold.

- Clock updates every second
- **SET CLOCK:** edit fields → **Save** writes RTC
- Light AUTO uses `lightHour` / `lightDuration`

## 4. Sensors

- SHT30: GRH T/RH, one read per ~2 s
- DS18B20: INC zone
- `ERR!` when disconnected; SHT30 T and H usually fail together

## 5. Outputs (no load)

**Boot:** all **LOW** for **3 s**.

**Critical fault** (NO NVS or all sensors ERR!): D2, D6–D9 **LOW**; logic blocked.

| Check | Expected |
|-------|----------|
| Idle | D2, D6–D9 **LOW** |
| Critical fault | All outputs **LOW** |
| GRH/HUM/INC FORCE ON (sensors OK) | D6/D7/D2 **HIGH** |
| FORCE ON (sensor error) | **LOW** |
| HUM MAN + `humError` | Humidifier **OFF**, no cycle |
| FAN / LIGHT FORCE ON | D8 / D9 PWM on |
| LIGHT Pct 0/50/100 | D9 duty changes |
| Max-on / over-temp | SSR off per limits in `firmware/Psi_Unit_Firmware/config.h` |

## 6. NVS

- Change setting, wait ~0.5 s, reboot → value kept
- Corrupt NVS → defaults + `NO NVS` + all outputs OFF
- `SETTINGS_VER` **25**; namespace `psi_cfg`

## 6b. RGB LED

- SET → RGB ON: green (OK), blue (SoftAP), orange/red (sensor error)
- RGB OFF: LED dark

## 6c. System info

- **SET → SYS**: firmware, uptime, RTC, heap, I2C; scroll to **BACK**

## 6d. Wi‑Fi (`WIFI_ENABLE=1`)

Default: SoftAP **off** until **SET → WiFi → Wi‑Fi SoftAP → ON**.

| Step | Expected |
|------|----------|
| Fresh boot | No Wi‑Fi icon; `dump wifi` inactive |
| SoftAP ON | Icon on main; `[WiFi] SSID=…`, URL in Serial |
| Phone → `http://192.168.4.1` | Dashboard opens (no extra login by default) |
| QR-code screen | Join QR + S:/P:/U: pages |
| Timeout | AP stops; menu shows OFF |
| SoftAP OFF / `wifi stop` | AP stays off until manual ON |
| Reboot after AP was ON | AP restores if user had enabled it |

## 6e. Sensor log (`SENSOR_LOG_ENABLE=1`)

- Needs flash **data** partition (LittleFS or FFat)
- SET → LOG → Enable, Interval, Save
- Web charts: 6h / 24h / 7d / 30d
- Boot: `Sensor log: … OK` or storage-unavailable message

## 7. Watchdog and heap

- Heap &lt; 20 KB: Serial `WARN heap low` every 30 s
- SoftAP + phone on dashboard 10+ min: no heap spam

## 8. Soak test

24 h: FAN/HUM cycles, SHT30 reconnect, RTC edit + reboot.

## 9. Serial CLI

115200, **Newline** (`SERIAL_CLI_ENABLE 1`):

1. Boot: `Build:`, `Features:`
2. `help` → command list
3. `dump` → snapshot blocks
4. Change setting → `dump settings` updated
5. FORCE ON → `dump outputs` shows channel on
6. `wifi stop` → AP off until `wifi start` or menu ON

See [ARCHITECTURE.md](ARCHITECTURE.md).
