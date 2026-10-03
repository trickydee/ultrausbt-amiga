# OLED UI style guide (ultrausbt-amiga)

**Audience:** firmware / docs maintainers.  
**Related code:** [`src/display/display.c`](../../src/display/display.c), [`src/display/display.h`](../../src/display/display.h).  
**Sibling pattern:** Apple ADB adapter Mode screen + `#` carousel (`ultrausbt-apple-adb-adapter`); adapted here for **three buttons only** (no `*` quick-toggle GPIO).

## Terminology

Use **screen carousel** for the `#`-advanced sequence of full-screen pages. Prefer that phrase over “menu cycle” or “screen list” in user-facing docs.

**In-page select** means Up/Down move a `>` cursor on the current page; `#` confirms the highlighted row (Settings).

## Screen carousel

| Mode | Sequence |
|------|----------|
| Amiga (USB host) | Splash → Devices → Map Devices → Settings → Splash |
| PC KBD (USB adapter / device role) | Splash → Settings → Splash |

Use on-screen names **Device Mode** (USB/BT → Amiga) and **Host Mode** (Amiga kbd → PC).

- **`#` (Middle)** advances the carousel when not on an in-page select action that consumes `#`.
- On **Settings**, `#` activates the highlighted row (does not advance the carousel). Use **Back** to return to Splash.
- **Devices** and **Map Devices** are host-only. While in PC KBD mode they must not appear in the carousel and `display_show_devices()` / `display_show_map_devices()` must no-op / redirect to Splash.

## Physical buttons (Up / Down / # modules)

| Key | Logical | Typical use |
|-----|---------|-------------|
| Up | Left | Port 1 cycle (Splash/Devices); Settings cursor up |
| Down | Right | Port 2 cycle (Splash/Devices); Settings cursor down; cancel confirm |
| # | Middle | Advance carousel; Settings confirm |

## Settings page

Single carousel page for privileged actions (no long hold chords):

1. **Clear BT pair** (Device Mode / Amiga host only) → confirm overlay (`#=yes`, Down=no) → `bluepad32_delete_pairing_keys()`
2. **Pair ON / Pair OFF** (label = state you will switch **to**) → `bluepad32_pairing_start()` / `stop()`
3. **Host Mode / Device Mode** (label = role you will switch **to**) → `usb_mode_request_toggle()` (persist + reboot)
4. **Back** → Splash (**default** selection when entering Settings)

Footer hint: `#=ok ^/v=sel`.

## Destructive / role-changing actions

- Prefer an **explicit confirm overlay** (Clear BT pair) or an obvious Settings row that reboots (USB role).
- Do **not** reintroduce Left+Right / Middle+Right **hold chords** for these settings actions.

## Documented chord exception

**Middle + Left (# + Up)** on Splash still toggles Bluetooth pairing on/off. Prefer **Settings → Pair ON / Pair OFF** when pairing has timed out or been disabled — clearer than the chord alone.

## Contrast with Apple ADB

| | Apple ADB | Amiga (this project) |
|--|-----------|----------------------|
| Carousel | `#` advances | Same |
| Mode switch | Mode page + `*` anywhere | Settings **USB:** row only (no `*`) |
| Clear pairings | `˄+˯` hold 5 s | Settings → Clear BT pair + confirm |
| Buttons | Up / Down / # / * | Up / Down / # only |

## Implementation notes

- Debounce edge detect on button release/press counters (existing `BUTTON_DEBOUNCE_COUNT` pattern).
- Keep Settings selection state (`settings_sel`) across redraws; clamp when entering PC KBD mode so Clear BT pair is not selected.
- Device-mode splash title is **Device Mode**; Host Mode splash title is **Host Mode** (not “PC KBD” / “Controller Mode”).
- Device-mode splash should point users at Settings (`# Settings`), not obsolete hold-combo text.
