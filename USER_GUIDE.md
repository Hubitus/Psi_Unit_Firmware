> **Languages:** English | [Русский](docs/ru/USER_GUIDE.md)

> **Psi_Unit** firmware **v0.1.0**. Build and flash — [README.md](README.md), [HARDWARE.md](HARDWARE.md), [BOM.md](BOM.md).

# User Guide

Operation of a finished device: OLED menu, Wi‑Fi, safety, errors.

**Contents**

- [Device purpose](#device-purpose)
- [First power-on](#first-power-on)
- [Connector markings on the enclosure](#connector-markings)
- [Load limits](#load-limits)
- [Safety: wiring and service](#safety-wiring-and-service)
- [Controls](#controls)
- [Main screen — what the lines mean](#main-screen)
- [Settings menu](#settings-menu)
- [Wi‑Fi and web UI](#wifi-and-web-ui)
- [Operating modes (Mod)](#operating-modes-mod)
- [Sensors](#sensors)
- [DS3231 RTC and backup battery (VBAT)](#ds3231-rtc-and-backup-battery-vbat)
- [Errors and indication](#errors-and-indication)
- [Factory defaults (after reset)](#factory-defaults-after-reset)
- [On-screen abbreviations](#on-screen-abbreviations)
- [Operation tips](#operation-tips)

---

## Device purpose

**Psi_Unit** is a local dual-zone climate controller. It:

- measures temperature and humidity (main zone — **GRH**);
- measures temperature in the second zone (**INC**, DS18B20 sensor);
- controls heating (**GRH** and **INC**), humidifier, fan, and lighting according to configured rules;
- stores settings in on-board internal memory (survives power loss);
- uses an RTC module (light schedule, log timestamps, and time display).

Control is via **OLED display**, **rotary encoder**, and optionally **web over Wi‑Fi** (SoftAP access point).

## First power-on

Short checklist before connecting loads:

1. **Wiring** — mount sensors and power lines with device power **off** ([safety](#safety-wiring-and-service)).
2. **Power** — turn the device on; the main screen should show time and sensor readings (or **ERR!** / **RTC ERROR!** — see [errors](#errors-and-indication)).
3. **Time** — **SET → TIME**, or after enabling Wi‑Fi, web **Settings → Clock & date → Sync from this phone** ([details](#wifi-and-web-ui)).
4. **Channel modes** — in menus **INC**, **GRH**, **HUM**, **FAN**, **LGT**, check **Mod** (after [reset](#factory-defaults-after-reset) all channels are **OFF**).
5. **Wi‑Fi** (optional) — **SET → WiFi → Wi-Fi SoftAP → ON**, then **QR-code** or address **`http://192.168.4.1`** ([details](#wifi-and-web-ui)).

Short labels on the OLED are explained in the [abbreviations glossary](#on-screen-abbreviations) at the end of this document.

## Connector markings on the enclosure

The device enclosure has markings above the connectors (vertical bars).

| Bars on enclosure | Channel | Load |
|-------------------|---------|------|
| **I** | INC | Second-zone heating (via DS18B20) |
| **II** | GRH | Main heating |
| **III** | HUM | Humidifier |
| **IIII** | FAN | Fan |
| **IIIII** | LGT | Lighting (PWM) |

**Sensor connectors** are not labeled separately: incorrect connection is prevented **mechanically** — each sensor type has its own connector with a **different number of pins** (the connector will not fit the wrong port).

**Power** is marked with the **standard power-on symbol** (circle with a horizontal line at the top). Connect the power cord only to this connector.

## Load limits

The controller only switches modules on/off; it does **not** limit current in the power circuit. Below are **conservative** limits for typical Chinese modules (no quality guarantee).

| Connector | Channel | Module | Load | Recommended max. power |
|-----------|---------|--------|------|------------------------|
| **I** | INC | SSR G3MB-202P, 230 V AC | 2nd-zone heating | **~230 W** (≤ **1 A**) |
| **II** | GRH | same | Main heating | **~230 W** (≤ **1 A**) |
| **III** | HUM | same | 230 V humidifier | **~230 W** (≤ **1 A**) |
| **IIII** | FAN | MOSFET DC | 12 V fan (default) or 5 V fan | **~60 W** (≤ **5 A**) at 12 V |
| **IIIII** | LGT | same, PWM | 12 V light (default) or 5 V light | **~36 W** (≤ **3 A**) at 12 V |

**SSR:** coil supply **5 V**; switched side **AC 230 V only**, not DC; load **≥ ~0.1 A**, otherwise it may not turn off; in OFF state leakage is possible (~1 mA). **MOSFET:** driver logic **3.3 V** from Arduino; load supply **12 V by default** or **5 V** if the PCB **Solder Bridge** is set to 5 V ([HARDWARE.md](HARDWARE.md#power-distribution)). With PWM, light runs hotter — limit is lower than for FAN.

**GRH** and **INC** use the same heating mat: **Lerway Indoor Greenhouse Heating Mat** 52.7×25.4 cm (~21 W), **one per zone** (connectors **II** and **I**). Full parts list — [BOM.md](BOM.md).

**230 V** wiring — only with the device **fully de-energized** ([safety](#safety-wiring-and-service)).

## Safety: wiring and service

| What you connect | Device power | Notes |
|------------------|--------------|-------|
| **~230 V** load (heating **GRH**, humidifier **HUM**, zone **INC** via SSR; any mains power circuit) | **Must be off** | Work only with **full de-energization** of the device **and** de-energized/open switched load. Connection or rewiring under voltage is **forbidden** — risk of electric shock and short circuit. |
| Low-voltage lines (**FAN**, **LGT** — MOSFET load, 12 V or 5 V per PCB bridge) | **Strongly recommended off** | Lower risk than 230 V, but with power on the relay may actuate during wiring. |
| **Sensors** (FS400-SHT30, DS18B20, connectors with different pin counts) | **Strongly recommended off** | Sensors run on **3.3 V** from Arduino (I2C / OneWire); hot-plugging is **not** a normal operating mode. With power on, bus spikes, **ERR!** on screen, and poll failures are possible; in careless wiring — sensor or I2C line damage. After correct connection with power on, firmware **usually** restores communication on its own (see SHT30/DS18B20 polling). **Recommendation:** disconnect device power, connect the sensor, verify the correct connector, then apply power. |
| **CR2032** battery on RTC module | **Must be off** | See [battery replacement](#battery-replacement-and-check). |

## Controls

| Action | Result |
|--------|--------|
| **Short press** of button | Confirm / enter menu item |
| **Long press** (~1 s) | Back / cancel (depends on screen) |
| **Encoder rotation** | Select item or change value |

On the **main screen**:

- short press → main menu (**MENU**): item order **INC**, **GRH**, **HUM**, **FAN**, **LGT**, **SET**, **BACK**;
- long press → quick light on/off ( **OVR** mode on the LGT line). This is temporary; light settings in memory are not changed; another long press clears OVR; after **2 h** (default) OVR clears automatically.

If the menu is idle for about 5 minutes, the controller returns to the main screen. The display may turn off after inactivity — parameter **Sleep** in **SET → DISP**; any encoder action wakes the display.

## Main screen — what the lines mean

```
  HH:MM:SS   [WiFi]        DD/MM/YY
  ─────────────────────────────────────
  INC: 23.4°C  │  GRH: 24.1°C
               │  HUM: 65%
  ─────────────────────────────────────
  INC   LGT   GRH        ← 3×2 grid
  FAN   ---   HUM        ← inversion = on
```

- Between time and date — **Wi‑Fi icon** when SoftAP is enabled.
- **GRH** — main zone temperature; **HUM** — humidity (%).

- **ERR!** instead of a number — sensor is not providing valid data (see Errors section).
- **RTC ERROR!** — clock unavailable; light schedule in AUTO does not work until time is set (**SET → TIME**).
- **NO NVS** at the bottom — settings memory unavailable; after reboot factory values apply, changes may not persist.
- Mode label: **AUT** (auto), **ON** (forced on), **OFF** (forced off), **MAN** (manual cycle — humidifier only).
- **OVR** on LGT — quick light from main screen is active.
- Device line **inverted** (white background) — output is **currently on**.

## Settings menu

Main menu → select section → submenu → change parameter → short press **saves** value (written to memory after ~0.5 s).

| Section | What is configured |
|---------|-------------------|
| **INC** | Mod (mode), Tmp (target °C), THy (hysteresis **0.1…5.0** °C) — via DS18B20 |
| **GRH** | Mod, Tmp, THy (hysteresis **0.1…5.0** °C) — main heating (SHT30) |
| **HUM** | Mod, Hum (%), HHy (%), Max (min), Wrk + Unt (on), Rst + Unt (pause) |
| **FAN** | Wrk + Unt (on), Rst + Unt (pause), Mod |
| **LGT** | Mode, Start (start hour), Duration (h), Brightness (PWM, %, step 10 %) |
| **SET** | WiFi, RGB, DISP, TIME, LOG (if in firmware), SYS, RESET, BACK |

**Ranges:** temperature hysteresis (**GRH**, **INC**) — **0.1…5.0 °C**; light brightness (**LGT Brightness**) — step **10 %** (OLED and web).

In **INC**, **GRH**, and submenus **WiFi** / **RGB** / **LOG**, list lines are spaced slightly wider (~12 px line spacing).

**SET → WiFi:** **Wi-Fi SoftAP** — access point on/off; **QR-code** — QR and SSID/password/IP text; **Timeout** — SoftAP auto-off (min; **Never** = no limit). Full procedure — section [Wi‑Fi and web UI](#wifi-and-web-ui).

**SET → LOG:** **Enable** — sensor history logging to flash; **Interval** — 5–15 min; **Save** — apply (in web: toggle and interval buttons save on press).

**SET → RESET:** confirmation “YES Reset default!” — select YES with encoder, short press performs reset.

**SET → TIME:** year/month/day/hour/minute fields; short press — select field → edit with encoder → next field; at end **Save** / **BACK** writes time to RTC or cancels. Alternative — sync from phone via web (see below).

**SET → SYS:** scrollable diagnostics list; section headers shown by **inversion**; **BACK** at end of list (scroll to end with encoder → short press). Long press — exit at any time.

## Wi‑Fi and web UI

The controller creates its **own Wi‑Fi network** in **SoftAP** mode (*Soft Access Point*). In the device menu this mode is labeled **WiFi**; in web settings — **Wi‑Fi Soft Access Point**. The device does **not** connect to your home router — only you connect to its network from a phone or laptop to open the settings page in a browser.

### 1. Enabling the access point (SoftAP)

1. On OLED: **main screen** → short press → **SET** → **WiFi**.
2. Item **Wi-Fi SoftAP** → **ON** → short press (save).
3. Wait a few seconds. Signs of success:
   - on main screen between time and date — **Wi‑Fi icon**;
   - RGB (if enabled): green “breathing” changes to blue when sensors are OK;
   - in Serial Monitor (115200): lines `[WiFi] SSID=…`, `Pass=…`, `URL=http://192.168.4.1`.

By default Wi‑Fi does **not** start on power-up — enable manually: **SET → WiFi → Wi-Fi SoftAP → ON**.

The **WiFi** menu item shows the **actual** SoftAP state (ON only if the access point is really running), not a stale value from memory.

**WiFi OFF** (or a Serial Monitor command — see [README.md](README.md)) turns SoftAP off and remembers “user turned off” — the controller does **not** bring the network up again every 5 s.

If you previously enabled SoftAP and saved the “on” preference, after **reboot** the device Wi‑Fi network may **restore automatically** (until you set WiFi OFF or **Timeout** expires).

### 2. Network name and password

| Method | Where to look |
|--------|---------------|
| **QR on OLED** | SET → WiFi → **QR-code** — first screen: QR for Wi‑Fi connection; encoder rotation — pages **S:** (SSID), **P:** (password), **U:** / **http://** (address) |
| **Serial Monitor** (USB) | Network name `SSID=PsiUnit-XXXX`; password — on OLED (**QR-code**) or via USB diagnostics (see [README.md](README.md)) |
| **Web** | **Settings → Wi‑Fi** — SSID, IP, and QR; password is **not shown** in the browser |

- **SSID:** `PsiUnit-` + 4 hex characters (unique per board), e.g. `PsiUnit-A3F2`.
- **Password:** 8 characters, **stored in board memory** — same password on each WiFi enable.
- **Browser address:** **`http://192.168.4.1`** (not `https`).

### 3. Connecting a phone or computer

**Android / iPhone**

1. Settings → Wi‑Fi → select network **PsiUnit-XXXX**.
2. Enter password (or scan QR from controller screen).
3. If the system says “no internet” — that is **normal**: the network is only for device access, not internet. Stay on this network.
4. Open browser (Chrome, Safari…) and enter: **`http://192.168.4.1`**

**Windows / macOS**

1. Connect to **PsiUnit-XXXX** in the Wi‑Fi list, enter password.
2. Browser: **`http://192.168.4.1`**

If the page does not open: verify WiFi ON, you are on PsiUnit network (not home Wi‑Fi), address starts with **`http://`**, no spaces.

**Web login:** by default no separate browser password — the Wi‑Fi network password is enough.

### 4. Main page (Dashboard)

Opens at **`http://192.168.4.1/`** — dark mobile layout, interface in **English**.

| Element | Purpose |
|---------|---------|
| Cards **INC / GRH °C / GRH %** | Current readings; **ERR** — sensor error; **tap** — history (chart) if Log is enabled |
| **Time** | Time and date from RTC |
| Tiles **GRH, HUM, FAN, LGT, INC** | Output state (highlight = on) and mode (AUT/ON/OFF/MAN); **tap** — that channel’s settings page |
| **Light override (OVR)** | Same as long press on OLED main screen — temporary light |
| **Settings** | General settings (clock, display, RGB, Wi‑Fi, log) |
| **System Info** | Diagnostics (like **SET → SYS** on OLED) |

Data refreshes automatically (~2 s). Header shows firmware version and **uptime**. On connection loss a red bar **No connection to device** appears at the top.

### 5. Settings pages

Path: **Settings** on main page or tap a channel tile on Dashboard.

- Fields match OLED menu (mode, setpoints, timers, light brightness, etc.).
- Top line **Live:** — current output state (ON/OFF and mode AUT/ON/OFF/MAN).
- **Modes** (AUTO | ON | OFF | MAN) — button row; **saved immediately on press**.
- **Toggles** (single ON/OFF button): display rotate/invert, RGB LED, Wi‑Fi SoftAP, log recording — also **on press**.
- **Multi-choice** (buttons, not dropdowns): time units (Sec/min), Wi‑Fi timeout, log interval (5/10/15 m) — **on press**.
- **Numeric fields** (temperature, hysteresis, timers, screen sleep, **OLED / RGB / LGT** brightness…) — **number button** → tap opens **wheel** (like iPhone); then **Save** (~0.5 s to NVS).
- **Cancel** — discard unsaved numeric edits.
- **Home** — return to main page.

**Settings** section (hub):

| Item | Contents |
|------|----------|
| **Clock & date** | Clock sync (see below) |
| **Display** | Slp (number + Save), OLED brightness (wheel %, step 5, + Save), rotate/invert (toggles) |
| **RGB status LED** | On/off (toggle), brightness (wheel %, step 10, + Save) |
| **Wi‑Fi** | SoftAP (toggle), timeout (buttons), SSID/password/QR |
| **Sensor history log** | Enable (toggle), interval 5/10/15 m (buttons), clear data |
| **Factory reset** | Reset setpoints (like **SET → RESET**) |

### 6. Clock sync (Clock & date)

Needed if OLED shows **RTC ERROR!** or time is wrong — **light schedule** in AUTO and sensor log timestamps depend on it.

**Via Wi-Fi:**

1. Connect to device WiFi and open **`http://192.168.4.1`**
2. **Settings** → **Clock & date**
3. Compare **Phone time** and **Device time (RTC)** (time in DS3231 chip)
4. Press **Sync from this phone** — phone time is written to RTC
5. On success — green message **Clock synced successfully**; time on OLED main screen and **Time** card updates

Uses **local time** of phone/computer. Time zone is whatever is set on your device.

**Via OLED:** **SET → TIME** (manual entry) — see above.

By default the controller applies **EU daylight saving time** rules automatically.

### 7. Sensor history (charts)

If **Sensor log** is enabled in firmware and **SET → LOG → Enable ON**:

- On main page **tap** INC, GRH °C, or GRH % card.
- Chart opens; period buttons **6 h / 24 h / 7 d / 30 d**.

Logging to flash every **5–15 min** (configurable); about **30 days** retained at minimum interval.

### 8. Wi‑Fi auto-off (Timeout)

**SET → WiFi → Timeout** or web **Settings → Wi‑Fi → Soft Access Point auto-off timeout**:

| Value | Behavior |
|-------|----------|
| **Never** (0) | SoftAP runs until you set **WiFi OFF** |
| **30 / 60 / 120 / 240 min** | SoftAP turns off after selected time; **WiFi** menu shows OFF (preference saved) |

**Default** timeout is **30 min** — if the access point turns off by itself, set **Never** for continuous operation.

After SoftAP off (manually, by Timeout, or via Serial Monitor) web is unavailable until **WiFi ON** again.

### 9. Limitations

- Up to **4** Wi‑Fi clients at once.
- No remote access over the internet — only near the device, on its network.
- Web and OLED change the **same** settings; last saved value applies to both.
- MQTT and home Wi‑Fi connection are **not supported**.

## Operating modes (Mod)

Common modes (where available):

| Mode | Behavior |
|------|----------|
| **AUTO** | Automatic control by sensor or schedule (see below) |
| **ON** | Output on continuously (with sensor-error limits) |
| **OFF** | Output off |
| **MAN** | **HUM** (and fan cycle): on — pause per **Wrk/Rst** |

### GRH — heating via SHT30 (main zone)

- **AUTO:** on when temperature **below** (target − hysteresis); off when **target reached**; between thresholds previous state is kept (no “chatter”).
- **ON:** heating on if no GRH sensor error.
- On GRH error heating in AUTO/ON **does not turn on**, and if already on — turns off.

### HUM — humidification via SHT30 humidity

- **AUTO:** like heating, but by **humidity** (%); **Max** limits continuous run (minutes), then humidifier off until next logic cycle.
- **MAN:** repeating cycle: Wrk (run) → Rst (pause); Wrk and Rst have **separate** units (sec/min), like the fan. **Max** — always in minutes. On **humidity error** cycle **does not run**, humidifier **OFF** (no SSR chatter).
- **ON:** on without AUTO if no humidity error.

### FAN — fan

- **AUTO:** work/pause cycles per Wrk and Rst (each has its own Unt: sec or min).
- **ON / OFF:** continuously on / off.

### LGT — lighting (PWM)

- **AUTO:** on in interval **[Start … Start+Duration)** per RTC hours (24-hour format). Brightness — **Brightness** (0–100%).
- **ON / OFF:** continuously on / off at set brightness.
- **OVR** from main screen — forced light over menu mode; clears after **2 h** (default) or on GRH error.

### INC — second-zone heating (DS18B20)

- **AUTO / ON / OFF** — same hysteresis logic as **GRH**, but on **INC** line with its own Tmp/THy.
- On DS18B20 error INC heating in AUTO/ON **does not turn on**, and if already on — turns off.

## Sensors

| Label | Sensor | Purpose |
|-------|--------|---------|
| **GRH** | FS400-SHT30 | Main temperature; also **H** — humidity |
| **INC** | DS18B20 | Second-zone temperature |
| Clock | DS3231 (RTC) | On-screen time and light schedule |

- Readings are **smoothed** (EMA), update about every **2 s**; SHT30 temperature and humidity — **one** I2C read (`readBoth`).
- On brief failure the controller **reconnects** SHT30 and DS18B20 automatically (intervals 2 → 5 → 10 → 30 s).
- After **3 consecutive failed** reads or **10 s** without valid data the value is invalid → **ERR!** (for SHT30 T and H usually fail **together**).

## DS3231 RTC and backup battery (VBAT)

The **DS3231** module keeps time and calendar when controller power is off via a separate battery on the RTC board (**2032** holder, 20 mm). DS3231 backup current is on the order of **microamps**, so with a sound circuit and fresh cell the battery usually lasts many years.

### Which battery to use

Use a lithium coin cell **CR2032** (~3.0 V, **non**-rechargeable), **2032** holder (20 mm). Use a known brand, not expired.

The RTC module is powered from Arduino **3.3 V**. Many DS3231 boards (including ZS‑042) ship with a **charging** circuit from **VCC** (resistor + diode). If you insert a standard **CR2032**, the module may **try to charge it** — the cell swells and fails in **1–3 years** instead of expected **7–10 years**. It is recommended to **disable charging**: on typical boards remove resistor **102** (1 kΩ) or, on some boards, **201** (200 Ω) near the diode in the battery path, so no current flows from **VCC** to the cell.

**VBAT** on DS3231 is rated roughly **2.3–3.7 V**; CR2032 matches this.

### When to replace the battery

Signs of a **discharged or faulty** cell (with sound I2C bus and DS3231 module):

- after power off, time **resets** or jumps years backward;
- **RTC ERROR!** on screen (clock unavailable on I2C — see below) though it worked before;
- **LGT AUTO** schedule triggers at wrong times;
- cell is **swollen**, leaking, or has been in the holder **many years** without replacement.

**RTC ERROR!** in firmware primarily means missing or failed communication with DS3231 (I2C, soldering, module). A dead battery is a common cause of time loss when mains power drops, but not the only one. If the error is constant with power on — check I2C and RTC module, not only the battery.

### Battery replacement and check

1. Disconnect controller power.
2. Remove 4 M2 screws securing the enclosure lid, carefully lift the lid and disconnect the display ribbon from the main board. The RTC is on the enclosure lid, next to the display module.
3. Install a new **CR2032** battery.
4. Apply power → **SET → TIME** or **web → Clock & date → Sync from this phone** — set correct date and time.
5. Verification: disconnect power for **5–10 minutes**, power on again — time and date should be preserved.


## Errors and indication

| Sign | Meaning |
|------|---------|
| **ERR!** on main screen | No data from that sensor |
| **RTC ERROR!** | No DS3231 communication on I2C — check module and bus; after recovery set time in **SET → TIME**. Time loss on power off — see [RTC battery](#ds3231-rtc-and-backup-battery-vbat) |
| **NO NVS** | Settings not saved to flash; **all outputs forced OFF** (see critical fault below) |
| RGB **green “breathing”** | Sensors OK, Wi‑Fi off (**SET → RGB → Enable ON**) |
| RGB **blue “breathing”** | SoftAP active, sensors OK |
| RGB **orange/red** | Sensor error (priority over blue Wi‑Fi) |
| RGB off | LED OFF in RGB menu |

### Quick diagnostics

| Symptom | What to check | Action |
|---------|---------------|--------|
| **ERR!** on GRH or INC | Wiring, sensor power | Reconnect with device **de-energized**; usually recovers on its own |
| **RTC ERROR!** | DS3231 module, CR2032 battery | **SET → TIME** or web Sync; see [RTC battery](#ds3231-rtc-and-backup-battery-vbat) |
| **NO NVS** | Settings memory | All outputs OFF; reboot; if repeated — service |
| Wi‑Fi turned off by itself | **Timeout** | **SET → WiFi → Never** or **WiFi ON** again |
| All outputs OFF, sensors OK | Critical fault | Check **NO NVS** and **ERR!** on all three channels (GRH °C, GRH %, INC) |

**Startup safety:** first **3 s** after power-on all SSRs and relays are **forced off**, then selected modes run.

**Critical fault:** if **NO NVS** (settings memory unavailable) **or** **ERR!** simultaneously on all three channels (GRH °C, GRH %, INC) — **all outputs OFF**: heating, humidifier, INC, fan, light, and quick light (**OVR**). Channel automation does not run until the condition clears (NVS OK again or at least one sensor recovers).

**AUTO/ON protection (single ERR!):** heating (GRH), humidification (HUM), and INC **do not turn on** on their sensor error, and if on — turn off. Fan and scheduled light **may run** if no critical fault.

**Failsafe (each cycle):** if SSR/relay is on during its sensor error — pins are **forced off**. **HUM MAN** on humidity error: humidifier OFF, work/rest cycle **does not run** (no SSR chatter). **Light OVR** clears on GRH error.

**Additional protection (default):** heating SSRs auto-off after **4 h**; above **35 °C** emergency shutoff; light **OVR** clears after **2 h**.

## Factory defaults (after reset)

**SET → RESET** restores channel and display setpoints to firmware factory values. **Wi‑Fi** and **sensor log** are stored separately and are **not** cleared by RESET.

| Parameter | Value after RESET |
|-----------|-------------------|
| **Mod** all channels (**INC**, **GRH**, **HUM**, **FAN**, **LGT**) | **OFF** |
| Heating target **GRH** / **INC** (**Tmp**) | **25.0 °C** |
| Hysteresis **THy** (**GRH**, **INC**) | **1.0 °C** |
| Humidity target **HUM** (**Hum**) | **60 %** |
| Humidity hysteresis **HHy** | **5 %** |
| **HUM Max** (max. continuous run) | **10 min** |
| **HUM** MAN cycle: **Wrk** / **Rst** | **5 min** / **25 min** |
| **FAN** cycle: **Wrk** / **Rst** | **15 sec** / **10 min** |
| Light **LGT**: **Start** / **Duration** / **Brightness** | **08:00** / **12 h** / **100 %** |
| Screen sleep **Sleep** | **2 min** |
| OLED brightness | **50 %** |
| Display rotate / invert | **off** |
| RGB | **on**, brightness **100 %** |

## On-screen abbreviations

The OLED has limited space — some sections use shortened labels; in **SET**, **LGT**, **DISP**, **RGB**, **LOG**, and **WiFi** full words are used. Below are labels as shown on screen. When in doubt, use this table.

### Channels, zones, and main menu

| On screen | Meaning |
|-----------|---------|
| **MENU** | **Main menu** title |
| **INC** | Second-zone heating (DS18B20); menu item and main screen line |
| **GRH** | **Main** zone heating (SHT30) — menu item, **temperature** label, and heating tile on main screen |
| **HUM** | Humidifier (menu and main screen) |
| **FAN** | Fan |
| **LGT** | Lighting (PWM brightness) |
| **SET** | System settings: Wi‑Fi, display, clock, reset, etc. |
| **BACK** | Back (main menu → main screen; submenu → level up) |

### Modes (Mod) and statuses

| On screen | Meaning |
|-----------|---------|
| **AUTO** | Automatic by sensors and setpoints (in mode list in menu) |
| **AUT** | Same as **AUTO** — shortened on main screen for channel |
| **ON** | Forced **on** |
| **OFF** | Forced **off** |
| **MAN** | Manual **cycle** on/pause (humidifier **HUM** only) |
| **OVR** | Temporary **forced light** from main screen (long press), does not change **LGT** schedule in menu |

### Common submenu parameters

| On screen | Meaning |
|-----------|---------|
| **Mod** | Channel mode (AUTO / ON / OFF / MAN) |
| **Tmp** | Target **temperature** (°C) |
| **THy** | Temperature **hysteresis** (°C) — on/off band |
| **Hum** | Target **humidity** (%) |
| **HHy** | Humidity **hysteresis** (%) |
| **Max** | Max. **continuous** humidifier run (min), then pause |
| **Wrk** | **Work** time in MAN cycle (sec or min) |
| **Rst** | **Pause** time in MAN cycle (**not** settings reset!) |
| **Unt** | Time **unit** for Wrk/Rst: **sec** / **min** in list; when editing — **SEC** / **MIN** |
| **Mode** | Mode (full word in **LGT**) |
| **Start** | Light **start** hour (0–23), in **LGT** menu |
| **Duration** | Light **duration** (hours), in **LGT** menu |
| **Brightness** | **Brightness** (OLED, RGB, **LGT** light) |

### SET — system items

Order in **SET** menu (3×3 grid, left to right, top to bottom): **WiFi**, **RGB**, **DISP**, **TIME**, **LOG** (if enabled in firmware), **SYS**, **RESET**, **BACK**.

| On screen | Meaning |
|-----------|---------|
| **TIME** | **Date and time** setup (RTC); edit screen — **SET CLOCK** |
| **DISP** | **Display** submenu |
| **Sleep** | Screen **sleep**: idle minutes until off (**0** = never off), in **DISP** |
| **Rotate** | **Rotate** image 180°, in **DISP** |
| **Invert** | Display color **invert**, in **DISP** |
| **RGB** | On-board **RGB** indicator submenu |
| **Enable** | On/off (RGB, LOG) |
| **WiFi** | **Wi‑Fi** submenu (SoftAP access point) |
| **Wi-Fi SoftAP** | Device **own Wi‑Fi network** on/off |
| **QR-code** | QR and **SSID / password / address** text |
| **Timeout** | Wi‑Fi auto-off after N min; **Never** — no limit |
| **LOG** | Sensor **log** submenu |
| **Interval** | Log record **interval** (5–15 min) |
| **Save** | Save (LOG, clock — **SET CLOCK**) |
| **SYS** | **System** information |
| **RESET** | **Reset** setpoints to factory (**do not** confuse with **Rst** on HUM/FAN!) |
| **BACK** | Back |

On **QR-code** pages (encoder rotation): **S:** — network name (SSID), **P:** — password, **U:** / **http://** — browser address.

### Messages and service (SET → SYS)

| On screen | Meaning |
|-----------|---------|
| **ERR!** | No valid data from **sensor** |
| **RTC ERROR!** | No communication with **DS3231** clock |
| **NO NVS** | On-board **settings** memory not working |
| **Ver** / **Bld** | Firmware version / build date |
| **Up** | Uptime since power-on (short: `45s`, `12m 30s`, `2d 5h`…) |
| **Heap** / **Min** | Free / minimum controller **memory** |
| **I2C** | Sensor and RTC bus state |
| **Rst** (in **Sys**) | Last chip **reset reason** (not settings reset) |

## Operation tips

### Connecting from a phone

Full procedure for SoftAP, QR code, and address **`http://192.168.4.1`** — in section [Wi‑Fi and web UI](#wifi-and-web-ui).  
**Tip:** after first login, add a home-screen shortcut (Android: browser menu → “Add to Home screen”; iPhone: Share → “Add to Home Screen”).

### General recommendations

1. Before first use verify time (**SET → TIME** or **web → Clock & date → Sync from this phone**) and all channel modes. Wire **230 V** loads and sensors with device power **off** (see [safety when wiring](#safety-wiring-and-service)).
2. On **ERR!** check sensor wiring and power; after recovery readings and RGB return on their own.
3. Diagnostics: **SET → SYS** or web **System Info**; with USB connected — Serial Monitor commands (see [README.md](README.md)).
4. Phone setup (near device): enable SoftAP and connect via QR (see [Wi‑Fi](#wifi-and-web-ui)) or manually to `PsiUnit-XXXX` → **`http://192.168.4.1`**.

For post-flash assembly verification — [TEST_CHECKLIST.md](TEST_CHECKLIST.md).
