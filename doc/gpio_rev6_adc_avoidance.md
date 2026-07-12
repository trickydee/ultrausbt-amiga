# GPIO Rev 6 — Avoid ADC Pins (Direct 5V / No Level Shifters)

# Board revision (pin map in `src/config.h`)

| Revision | Hardware | Port 2 fire/B2/B3 | OLED buttons L/M/R | Level shifters |
|----------|----------|-------------------|--------------------|----------------|
| **5** (default build) | Current shipping PCB | GPIO **26 / 27 / 28** | GPIO **18 / 17 / 16** | Yes (`ENABLE_LEVEL_SHIFTER=1`) |
| **6** | Pico 2 direct-5V PCB | GPIO **16 / 17 / 18** | GPIO **26 / 27 / 28** (ADC) | No (`ENABLE_LEVEL_SHIFTER=0`) |

Firmware support for both maps is complete. Build Rev 6 with:

```cmake
add_compile_definitions(HIDPICO_REVISION=6)
```

Rev 6 swaps the Port 2 joystick lines onto the freed 5V-tolerant header GPIOs (16–18) and moves the OLED UI buttons onto the ADC pins (26–28), which only ever see 3.3V tactile switches. **GPIO 0/1 are reserved for the debug UART** (`DEBUG_UART_TX`/`DEBUG_UART_RX`) and carry no Amiga lines.

Do **not** flash Rev 6 firmware onto a Rev 5 board (wrong pins). Full PCB notes below.

**Branch:** developed on `feature/gpio-no-ttl-keyboard-in` (adopting the `feature/gpio-avoid-adc` Rev 6 layout); maps live in `config.h` for all builds.

## Problem (Rev 5)

On RP2350, only **GPIO 0–25** are 5V-tolerant. Rev 5 used **GPIO 26–28** (ADC0–ADC2) for Port 2 fire and buttons 2/3. Those pins are **not** safe for direct Amiga 5V without level shifters.

| DB-9 pin | Signal | Rev 5 GPIO | ADC? | RP2350 5V OK? |
|----------|--------|------------|------|---------------|
| 6 | Fire | **26** | ADC0 | No |
| 9 | Button 2 | **27** | ADC1 | No |
| 5 | Button 3 | **28** | ADC2 | No |

CD32 mode on Port 2 uses the same three lines (JOYMODE / CLOCK / DATA).

## Locked Rev 6 mapping

All Port 2 button/fire lines move to **5V-tolerant** header GPIOs (16–18, freed by relocating the OLED buttons). Directions (19–22) and Port 1 (unchanged) were already safe. The OLED UI buttons move onto the ADC pins (26–28) — they only ever carry 3.3V tactile switches, never Amiga 5V.

| DB-9 pin | Signal | Rev 5 GPIO | **Rev 6 GPIO** | Firmware define |
|----------|--------|------------|----------------|-----------------|
| 6 | Fire | 26 | **16** | `QM2_AMIGA_B1` / `JOY0_ATARI_FIRE` |
| 9 | Button 2 | 27 | **17** | `QM2_AMIGA_B2` (CD32 DATA) |
| 5 | Button 3 | 28 | **18** | `QM2_AMIGA_B3` (CD32 JOYMODE) |

Port 1 buttons remain on GPIO **2** and **3** (already non-ADC).

### OLED UI buttons (moved to ADC pins)

| Button | Rev 5 GPIO | **Rev 6 GPIO** | Firmware define |
|--------|------------|----------------|-----------------|
| Left | 18 | **26** | `GPIO_BUTTON_LEFT` |
| Middle | 17 | **27** | `GPIO_BUTTON_MIDDLE` |
| Right | 16 | **28** | `GPIO_BUTTON_RIGHT` |

ADC pins are fine here: they are inputs with pull-ups tied to 3.3V tactile switches, never exposed to Amiga 5V.

### Debug UART

GPIO **0** (`DEBUG_UART_TX`) and **1** (`DEBUG_UART_RX`) are reserved for the stdio debug UART on Rev 6 and carry no Amiga signals.

### CD32 (Port 2)

| CD32 role | DB-9 pin | Rev 6 GPIO |
|-----------|----------|------------|
| JOYMODE | 5 | **18** (`QM2_AMIGA_B3`) |
| CLOCK | 6 | **16** (`QM2_AMIGA_B1`) |
| DATA | 9 | **17** (`QM2_AMIGA_B2`) |

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
| 7 | Free |
| 15 | Free |
| 24 | Free |
| 26–28 | OLED UI buttons (ADC, 3.3V tactile switches) — **do not use for 5V Amiga lines** |
| 29 | ADC3 — avoid for 5V |

## PCB requirements

1. Reroute **Port 2 DB-9 pin 6** (fire) from Pico GP26 → **GP16**
2. Reroute **Port 2 DB-9 pin 9** (B2) from Pico GP27 → **GP17**
3. Reroute **Port 2 DB-9 pin 5** (B3) from Pico GP28 → **GP18**
4. Move the **OLED UI buttons** from GP16/17/18 → **GP26/27/28** (ADC pins; 3.3V tactile switches only)
5. Keep **GP0/GP1** for the debug UART (no Amiga lines)
6. Remove TXB0108 (or equivalent) level shifters on the Port 2 fire/button lines if present
7. **IOVDD must be powered** whenever Amiga 5V is on the joyport (RP2350 requirement)

Rev 5 boards **must not** flash Rev 6 firmware without the PCB changes — fire/buttons will drive the wrong pins.

## RP2040 (original Pico / Pico W)

RP2040 has **no** 5V-tolerant GPIOs. Rev 6 direct-wiring target is **Pico 2 / Pico 2 W** only. RP2040 builds should stay on Rev 5 with level shifters.

## References

- `src/config.h` — `HIDPICO_REVISION == 6` block
- `doc/amiga-code-atari-board-gpio-mappings.md` — Rev 5 baseline
- `doc/CD32_BUILD_SPEC.md` — CD32 pin roles
- `doc/gpio_allocation_plan.md` — Option 2 (GPIO 0/1 for Port 2 B2/B3)
- Raspberry Pi RP2350 A4: GPIO 0–25 5V-tolerant; 26–29 ADC only
