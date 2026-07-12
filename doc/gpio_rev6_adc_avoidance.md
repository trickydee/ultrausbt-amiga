# GPIO Rev 6 — Avoid ADC Pins (Direct 5V / No Level Shifters)

# Board revision (pin map in `src/config.h`)

| Revision | Hardware | Port 2 fire/B2/B3 | Level shifters |
|----------|----------|-------------------|----------------|
| **5** (default build) | Current shipping PCB | GPIO **26 / 27 / 28** | Yes (`ENABLE_LEVEL_SHIFTER=1`) |
| **6** | Future Pico 2 direct-5V PCB | GPIO **7 / 0 / 1** | No (`ENABLE_LEVEL_SHIFTER=0`) |

Firmware support for both maps is complete. Build Rev 6 with:

```cmake
add_compile_definitions(HIDPICO_REVISION=6)
```

Do **not** flash Rev 6 firmware onto a Rev 5 board (wrong pins; also GPIO 0/1 conflict with UART). Full PCB notes: [`gpio_rev6_adc_avoidance.md`](./gpio_rev6_adc_avoidance.md).

**Branch:** historically developed on `feature/gpio-avoid-adc` / bisect; maps live in `config.h` for all builds.

## Problem (Rev 5)

On RP2350, only **GPIO 0–25** are 5V-tolerant. Rev 5 used **GPIO 26–28** (ADC0–ADC2) for Port 2 fire and buttons 2/3. Those pins are **not** safe for direct Amiga 5V without level shifters.

| DB-9 pin | Signal | Rev 5 GPIO | ADC? | RP2350 5V OK? |
|----------|--------|------------|------|---------------|
| 6 | Fire | **26** | ADC0 | No |
| 9 | Button 2 | **27** | ADC1 | No |
| 5 | Button 3 | **28** | ADC2 | No |

CD32 mode on Port 2 uses the same three lines (JOYMODE / CLOCK / DATA).

## Locked Rev 6 mapping

All Port 2 button/fire lines move to **5V-tolerant** GPIOs. Directions (19–22) and Port 1 (unchanged) were already safe.

| DB-9 pin | Signal | Rev 5 GPIO | **Rev 6 GPIO** | Firmware define |
|----------|--------|------------|----------------|-----------------|
| 6 | Fire | 26 | **7** | `QM2_AMIGA_B1` / `JOY0_ATARI_FIRE` |
| 9 | Button 2 | 27 | **0** | `QM2_AMIGA_B2` (CD32 DATA) |
| 5 | Button 3 | 28 | **1** | `QM2_AMIGA_B3` (CD32 JOYMODE) |

Port 1 buttons remain on GPIO **2** and **3** (already non-ADC).

### CD32 (Port 2)

| CD32 role | DB-9 pin | Rev 6 GPIO |
|-----------|----------|------------|
| JOYMODE | 5 | **1** (`QM2_AMIGA_B3`) |
| CLOCK | 6 | **7** (`QM2_AMIGA_B1`) |
| DATA | 9 | **0** (`QM2_AMIGA_B2`) |

Directions stay on GPIO 19–22 (unchanged).

## Software defaults

| Setting | Rev 5 | Rev 6 |
|---------|-------|-------|
| `HIDPICO_REVISION` | **5** (CMakeLists default) | **6** (`-DHIDPICO_REVISION=6`) |
| `ENABLE_LEVEL_SHIFTER` | 1 (default) | **0** (default) |
| `HIDPICO_REV_ATARI_BOARD` | defined | defined |

Build Rev 6 firmware (new PCB only):

```bash
# In CMakeLists.txt:
add_compile_definitions(HIDPICO_REVISION=6)
```

Or keep Rev 5 as the default and pass an override if your build scripts support it.

## Free GPIO after Rev 6

| GPIO | Status |
|------|--------|
| 15 | Free |
| 23 | Free |
| 24 | Free |
| 26 | Free (was Port 2 fire) — **do not use for 5V Amiga lines** |
| 27 | Free (was Port 2 B2) |
| 28 | Free (was Port 2 B3) |
| 29 | ADC3 — avoid for 5V |

## PCB requirements

1. Reroute **Port 2 DB-9 pin 6** (fire) from Pico GP26 → **GP7**
2. Reroute **Port 2 DB-9 pin 9** (B2) from Pico GP27 → **GP0**
3. Reroute **Port 2 DB-9 pin 5** (B3) from Pico GP28 → **GP1**
4. Remove TXB0108 (or equivalent) level shifters on these lines if present
5. **IOVDD must be powered** whenever Amiga 5V is on the joyport (RP2350 requirement)

Rev 5 boards **must not** flash Rev 6 firmware without the PCB changes — fire/buttons will drive the wrong pins.

## RP2040 (original Pico / Pico W)

RP2040 has **no** 5V-tolerant GPIOs. Rev 6 direct-wiring target is **Pico 2 / Pico 2 W** only. RP2040 builds should stay on Rev 5 with level shifters.

## References

- `src/config.h` — `HIDPICO_REVISION == 6` block
- `doc/amiga-code-atari-board-gpio-mappings.md` — Rev 5 baseline
- `doc/CD32_BUILD_SPEC.md` — CD32 pin roles
- `doc/gpio_allocation_plan.md` — Option 2 (GPIO 0/1 for Port 2 B2/B3)
- Raspberry Pi RP2350 A4: GPIO 0–25 5V-tolerant; 26–29 ADC only
