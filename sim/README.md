# Desktop UI Simulator

**128×64** window (×4 scale) with the same UI as on device: menus, splash, settings. See [USER_GUIDE](../USER_GUIDE.md#settings-menu).

---

## Requirements

| Tool | Notes |
|------|-------|
| **CMake** ≥ 3.16 | [cmake.org/download](https://cmake.org/download/) — add to PATH |
| **Visual Studio Build Tools** | Workload **Desktop development with C++** |
| **SDL2 2.32.6** | [release-2.32.6](https://github.com/libsdl-org/SDL/releases/tag/release-2.32.6) → **`SDL2-devel-2.32.6-VC.zip`** (not SDL 3.x) |

Unpack SDL2, e.g. `C:\libs\SDL2-2.32.6\` with `include\`, `lib\`, `cmake\SDL2Config.cmake`.

---

## Build

```powershell
cd sim
mkdir build -Force
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DSDL2_ROOT="C:/libs/SDL2-2.32.6"
cmake --build . --config Release
```

Or after first configure: `.\build.ps1` from `sim/`.

**CMake GUI:** source `sim/`, build `sim/build`, set **SDL2_ROOT**, generate, build `psi_ui_sim` in **Release**.

Output: `sim\build\Release\psi_ui_sim.exe` (+ `SDL2.dll` copied automatically).

---

## Run

```powershell
.\sim\build\Release\psi_ui_sim.exe
```

| Key | Action |
|-----|--------|
| ↑ / ↓ | Encoder |
| Enter | Short press |
| Esc | Long press |
| Q | Quit |

---

## Architecture

| Layer | Device | Simulator |
|-------|--------|-----------|
| Display | `display_backend_u8g2.cpp` | `display_backend_sdl.cpp` |
| Input | `input_backend_hw.cpp` | `input_backend_sim.cpp` |
| Storage / RTC / sensors | Hardware | `sim_storage`, `sim_rtc`, `sim_domain` |

Shared: `display_ui`, `ui_input`, `settings_field`, `app_snapshot`, `app_commands`, `app_state`.

`config_sim.h` via CMake: no Serial CLI, version `v0.1.0-sim`, short splash.

## Limitations

- No Wi‑Fi; settings in RAM (lost on exit)
- Demo sensors: 25.0 °C / 24.2 °C / 58 %
- Relays not emulated (UI flags only)
- Firmware sketch: `firmware/Psi_Unit_Firmware/Psi_Unit_Firmware.ino` (not `sim/`)
