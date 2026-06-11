# BOM — Psi_Unit

> **Languages:** English | [Русский](docs/ru/BOM.md)

> Mains **~230 V AC** — [USER_GUIDE](USER_GUIDE.md#safety-wiring-and-service), [HARDWARE.md](HARDWARE.md).  
> Pinout — [HARDWARE.md](HARDWARE.md).

---

## Controller and interface

| Photo | Part | Qty | Notes |
|-------|------|-----|-------|
| <img src="images/Arduino%20Nano%20ESP32%20with%20headers.jpg" width="80" alt="Arduino Nano ESP32"> | **Arduino Nano ESP32** | 1 | Board **Arduino Nano ESP32**, esp32 core **3.0.7**. Settings in on-chip **NVS**. On-board **RGB** status LED. |
| <img src="images/OLED_Encoder.jpg" width="80" alt="OLED SH1106"> | **OLED SH1106 128×64** (I2C) | 1 | **A4/A5**, address **0x3C** (fallback **0x3D**). **U8g2**. I2C **100 kHz**. **3.3 V** from Arduino. |
| <img src="images/OLED_Encoder.jpg" width="80" alt="Rotary encoder"> | **Rotary encoder** (CLK / DT / SW) | 1 | **CLK → D4**, **DT → D3**, **SW → D5**. **Encoder** library. *Same photo as OLED.* |
| <img src="images/RTC%20DS3231.jpg" width="80" alt="DS3231 RTC"> | **DS3231 RTC module** (I2C) | 1 | **A4/A5**, address **0x68**. **RTClib**. **3.3 V**. [RTC battery](USER_GUIDE.md#ds3231-rtc-and-backup-battery-vbat). |
| <img src="images/CR2032.jpg" width="80" alt="CR2032"> | **CR2032** (RTC backup) | 1 | **VBAT** on DS3231. Disable charging on typical ZS-042 boards — [USER_GUIDE](USER_GUIDE.md#ds3231-rtc-and-backup-battery-vbat). Replace with power **off**. |

---

## Power switching modules

| Photo | Part | Qty | Notes |
|-------|------|-----|-------|
| <img src="images/SSR.jpg" width="80" alt="SSR G3MB-202P"> | **SSR G3MB-202P**, 1 ch, 5 V, HIGH-level (230 V AC) | 3 | **HIGH = on**. Boot **off** for **3 s**. **SSR_INC → D2**, **SSR_HEAT → D6**, **SSR_HUM → D7**. Coil **5 V**. **AC 230 V** only; load **≥ ~0.1 A**. Wire mains **de-energized**. [Load limits](USER_GUIDE.md#load-limits). |
| <img src="images/Mosfet.jpg" width="80" alt="MOSFET driver"> | **MOSFET driver** 5–36 V, HIGH-trigger | 2 | **RELAY_FAN → D8**, **RELAY_LIGHT → D9** (PWM). Logic **3.3 V**; load **12 V** default or **5 V** via PCB **Solder Bridge** — [HARDWARE.md](HARDWARE.md#power-distribution). |

---

## Power supply

| Photo | Part | Qty | Notes |
|-------|------|-----|-------|
| <img src="images/Mean%20Well%20IRM-10-12.jpg" width="80" alt="Mean Well IRM-10-12"> | **Mean Well IRM-10-12** (230 V → 12 V DC) | 1 | **12 V** bus: MOSFET loads (default **12 V**; **5 V** if bridge re-soldered), **MP1584EN** input, SSR **5 V**. |
| <img src="images/MP1584EN.jpg" width="80" alt="MP1584EN"> | **MP1584EN** step-down (12 V → 5 V, up to 3 A) | 1 | Controller **5 V**. Sensors, OLED, RTC, MOSFET logic — **3.3 V** from Arduino. |

---

## Sensors

| Photo | Part | Qty | Notes |
|-------|------|-----|-------|
| <img src="images/FS400-SHT30.jpg" width="80" alt="FS400-SHT30"> | **FS400-SHT30** (T + RH, enclosure) | 1 | I2C **A4/A5**, **0x44**. **3.3 V**. Zone **GRH**. Connect with power **off**. |
| <img src="images/DS18B20.jpg" width="80" alt="DS18B20"> | **DS18B20** (OneWire) | 1 | **A6** only (GPIO 13). Not D11, D12, A4, A5. **OneWire** + **DallasTemperature**. Zone **INC**. |
| <img src="images/Resistor_4_7_K.jpg" width="80" alt="4.7 kΩ resistor"> | **4.7 kΩ** (OneWire pull-up) | 1 | DATA → **3.3 V**. Sensor **3.3 V**, common GND — [HARDWARE.md](HARDWARE.md#ds18b20-wiring-and-libraries). |

---

## Loads (examples)

| Photo | Part | Qty | Notes |
|-------|------|-----|-------|
| <img src="images/Trixie%20Fogger%20Ultrasonic%20Mist%20Generator.jpg" width="80" alt="Trixie Fogger"> | **Trixie Fogger** ultrasonic humidifier (~25 W) | 1 | **HUM**, connector **III**, SSR 230 V. Max **~230 W** — [USER_GUIDE](USER_GUIDE.md#load-limits). |
| <img src="images/Lerway%20Indoor%20Greenhouse%20Heating%20Mat.jpg" width="80" alt="Lerway Heating Mat"> | **Lerway Indoor Greenhouse Heating Mat** 52.7×25.4 cm (~21 W) | 2 | **GRH** (connector **II**) and **INC** (connector **I**), SSR 230 V. Max **~230 W** per channel. |
| <img src="images/Fan_50x50.jpg" width="80" alt="50×50 fan"> | **12 V brushless fan** 50×50×15 mm | 1 | **FAN**, connector **IIII**. **12 V** default; **5 V** fan if PCB bridge set to **5 V**. Max **~60 W** at 12 V. |
| <img src="images/LED_Strip_12V.jpg" width="80" alt="12 V LED strip"> | **12 V LED strip** (~50 mm) | 1 | **LGT**, connector **IIIII**, PWM on **D9**. **12 V** default; **5 V** strip if bridge at **5 V**. Max **~36 W** at 12 V. |

---

## Connectors and switching

| Photo | Part | Qty | Notes |
|-------|------|-----|-------|
| <img src="images/C8_IEC_inlet.jpg" width="80" alt="C8 IEC inlet"> | **C8** IEC inlet (250 V / 2 A) | 4 | Mains input. [Connector markings](USER_GUIDE.md#connector-markings). |
| <img src="images/iec_c7_female_plug.jpg" width="80" alt="C7 IEC plug"> | **C7** IEC plug (2 pin, 250 V / 2.5 A) | 3 | Per wiring diagram. |
| <img src="images/2.1-x-5.5mm-DC-Power-Jack-Socket-Female-Panel-Mount-1.jpg" width="80" alt="DC jack 2.1×5.5 mm"> | **DC jack 2.1×5.5 mm** (panel) | 2 | 5–12 V load supply. |
| <img src="images/GX12-4.jpg" width="80" alt="GX12-4"> | **GX12-4** aviation connector (4 pin) | 1 set | FS400-SHT30 (power + I2C). |
| <img src="images/GX12-3.jpg" width="80" alt="GX12-3"> | **GX12-3** aviation connector (3 pin) | 1 set | DS18B20 (VCC, DATA, GND). |
| <img src="images/Rocker_switch_H8500VBBB-EN551_SPL.jpg" width="80" alt="Rocker switch"> | **Rocker switch** H8550VBBB-080W-ND | 1 | Mains switch. |
| <img src="images/C7_Power_Cord.png" width="80" alt="C7 power cord"> | **Power cord** 2 m, Euro, C7, black | 1 | Mains input connector only. |

---

Wiring: [HARDWARE.md](HARDWARE.md) · [PDF](docs/Psi_Unit_Wiring_Diagram.pdf) · [PNG](docs/Psi_Unit_Wiring_Diagram.png).
