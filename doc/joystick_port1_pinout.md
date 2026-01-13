# Joystick Port 1: DB-9 Pin to GPIO Mapping

## DB-9 Connector Pinout to GPIO Mapping (Revision 4)

Based on the KiCad schematic netlist mapping:

| DB-9 Pin | Signal (Mouse Mode) | Signal (Joystick Mode) | GPIO Pin | GPIO Signal | Description | Active State |
|----------|---------------------|------------------------|----------|-------------|-------------|--------------|
| 1 | V | UP | 10 | `QM1_AMIGA_V` | Vertical / Up direction | LOW (0) |
| 2 | H | DN | 9 | `QM1_AMIGA_H` | Horizontal / Down direction | LOW (0) |
| 3 | VQ | LF | 8 | `QM1_AMIGA_VQ` | Vertical Quadrature / Left direction | LOW (0) |
| 4 | HQ | RT | 7 | `QM1_AMIGA_HQ` | Horizontal Quadrature / Right direction | LOW (0) |
| 5 | b3 | b3 | 13 | `QM1_AMIGA_B3` | Button 3 | LOW (0) |
| 6 | b1 | b1 | 11 | `QM1_AMIGA_B1` | Button 1 (Fire) | LOW (0) |
| 7 | +5V | +5V | - | - | Power (not connected to GPIO) | N/A |
| 8 | GND | GND | - | - | Ground (not connected to GPIO) | N/A |
| 9 | b2 | b2 | 12 | `QM1_AMIGA_B2` | Button 2 | LOW (0) |

## Notes

### Direction Signals (Joystick Mode)
- **DB-9 Pin 1 (UP) - GPIO 10 (`QM1_AMIGA_V`)**: Vertical signal, used for up direction
- **DB-9 Pin 2 (DN) - GPIO 9 (`QM1_AMIGA_H`)**: Horizontal signal, used for down direction
- **DB-9 Pin 3 (LF) - GPIO 8 (`QM1_AMIGA_VQ`)**: Vertical quadrature signal, used for left direction
- **DB-9 Pin 4 (RT) - GPIO 7 (`QM1_AMIGA_HQ`)**: Horizontal quadrature signal, used for right direction

**Note:** In joystick mode, the direction signals use simple digital levels (active low).

### Quadrature Signals (Mouse Mode Only)
- **Horizontal Quadrature (HQ) - GPIO 7**: Used for mouse quadrature encoding, **not used in joystick mode**
- **Vertical Quadrature (VQ) - GPIO 8**: Used for mouse quadrature encoding, **not used in joystick mode**

### Button Signals
- All buttons are **active low** (LOW = pressed, HIGH = not pressed)
- **DB-9 Pin 5** (b3) = GPIO 13 (`QM1_AMIGA_B3`) = Button 3
- **DB-9 Pin 6** (b1) = GPIO 11 (`QM1_AMIGA_B1`) = Button 1 (Fire)
- **DB-9 Pin 9** (b2) = GPIO 12 (`QM1_AMIGA_B2`) = Button 2

## Important Notes

1. **Shared GPIO Pins**: Joystick Port 1 uses the same GPIO pins as the mouse interface (GPIO 7-13). They share the same physical connector on the Amiga.

2. **Active Low Signals**: All signals are active low:
   - Direction pins: LOW = direction active, HIGH = direction inactive or opposite direction
   - Button pins: LOW = button pressed, HIGH = button not pressed

3. **Joystick vs Mouse Mode**:
   - **Mouse mode**: Uses quadrature encoding (GPIO 7, 8, 9, 10 with quadrature sequences)
   - **Joystick mode**: Uses simple digital signals (GPIO 9, 10 for directions, GPIO 11-13 for buttons)
   - Quadrature signals (GPIO 7, 8) are kept inactive in joystick mode

4. **Direction Logic**:
   - For joystick mode, the H and V pins use simple digital levels:
     - H pin LOW = Left
     - H pin HIGH = Right (or no horizontal)
     - V pin LOW = Up
     - V pin HIGH = Down (or no vertical)

## Code Reference

- GPIO definitions: `src/config.h` (QM1_AMIGA_* macros)
- Joystick implementation: `src/platform/amiga/joystick_port1.c`
- Mouse implementation: `src/platform/amiga/quad_mouse.c`

