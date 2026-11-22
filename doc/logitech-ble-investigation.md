# Logitech BLE reconnection investigation

> Status update (2 November 2026): the upstream reconnect fix was submitted as
> [Bluepad32 PR #236](https://github.com/ricardoquesada/bluepad32/pull/236).
> The dated investigation and test notes below describe earlier stages; references
> to an unpublished PR or pending tests apply to those stages. Amiga and Apple ADB
> hardware tests succeeded, and the Atari MegaSTE upstream-versus-patched comparison
> confirmed reconnects with existing bonds. Jaguar hardware validation remains unconfirmed.

Branch: `feature/bluepad32-logi-mx-investigations`. Firmware version is 4.2.2;
identify this test build by the `[BT-TEST]` UART banners.

The longstanding Logitech reconnect problem predates our 60-second pairing window.
Two separate candidates: stopping BLE scans prevents discovery; Bluepad32 rejects
advertisements without a recognised HID appearance. Logitech reconnect packets
have now been captured: directed advertisements with an exact saved identity are
rejected by the appearance filter. Version 4.2.1 implements the targeted fix below.

## Temporary configuration

In `src/config.h`, `BT_PAIRING_ALWAYS_ON_TEST=1` disables the boot pairing timeout.
Pairing/scanning starts at boot and remains on; Settings can still switch it OFF.
`BT_LOGI_ADV_DEBUG` now defaults to **0** (advertisement logging off).
Set `BT_LOGI_ADV_DEBUG=1` to re-enable diagnostics; this registers a firmware-owned BTstack event observer that receives
advertisements independently of Bluepad32's filtering. No submodule changes,
active-scan changes, bond deletion, or authentication changes are introduced.
Restore both flags to 0 to restore normal timeout and disable these diagnostics.

## Capture procedure

1. Use the normal USB/BT-to-Amiga mode. Disable scanning on other retro adapters
   and temporarily disable Mac Bluetooth to isolate this adapter.
2. Pair the Logitech once if necessary. Do not clear keys between tests.
3. Record UART with the device working, switch only the peripheral off, wait for
   its disconnect message, then turn it on without pressing its pairing button.
4. Wait 15 seconds. If it fails, press its pairing button and capture that too.
5. Repeat with an adapter reboot, and with a mouse and keyboard separately.

Include boot banners, `[BLE-ADV]` lines and security/connection logs. In particular,
compare re-encryption using existing bonds versus new pairing. Pressing the
peripheral's pairing button does not by itself prove that new keys were generated.

`event`: 0=ADV_IND, 1=directed, 2=scannable, 3=non-connectable, 4=scan response.
`addr_type`: 0=public, 1=random (other numeric values retained verbatim).
`appearance_filter` predicts only the bundled Bluepad32 appearance check, not the
final connection outcome. `exact_bond=-1` means no exact identity-address match;
it does NOT rule out a bonded device using a resolvable private address.
`data` is the raw public advertisement, never stored pairing keys.

Logging is limited to four advertisement lines per second, repeating identical
address/type/payload combinations at most every five seconds in a 32-entry cache.
Rate-limited counts indicate incomplete captures; absence of a line is not proof
that no advertisement was sent. Passive scanning and controller-level filtering
also limit what reaches this observer. Nearby unrelated devices can appear.

## Intended subsequent design

Separate new-device pairing, known-device reconnection, and accepting no new
connections. Preserve the ability to run several retro adapters concurrently
without all of them racing to connect. Resolve saved BLE identities before
accepting reconnect advertisements without HID appearance. Validate Logitech,
Stadia and Xbox after any behavior change; avoid broad stack upgrades or timing
changes in this diagnostic step.

## Confirmed ERGO M575B capture and 4.2.1 fix

The power-cycle capture showed `DE:84:37:C5:16:F2`, event 1 (directed), empty
payload, appearance 0, and exact bond index 2. This repeated with scanning ON.
Pressing pairing changed the address to `...F3`, advertised mouse appearance
0x03c2, and caused a new pairing. Thus the missing-appearance filter blocks this
observed reconnect; bond storage was present in the same running session.

The reconnect patch accepts directed advertisements only when address AND address
type exactly match an existing LE database identity. Allowlist, RSSI and platform
admission checks remain. Encryption is still handled by the existing BTstack
security path. No keys are cleared by this patch. Unknown devices use the original
appearance filter. A database identity match is not itself authentication.

A reconnect has no class in its advertisement: classification is recovered from
HID Application Collections after HID service discovery and before selecting the
mouse/keyboard/gamepad parser. Missing, unsupported or mixed classes disconnect
rather than defaulting a keyboard to a gamepad. Since 4.2.2, the OLED name is restored from saved bond metadata. For older bonds
without metadata, Device Information Model Number is used when available; the
generic class name remains a last resort.

`patches/bluepad32-bonded-directed-reconnect.patch` is applied to a build-tree copy
by `cmake/bluepad32_reconnect.cmake`; the submodule stays untouched. A source hash
and zero-fuzz patch application fail configuration if the pinned source changes.
Use `./build-all.sh` for firmware builds. The system `patch` program is required.

Expected UART sequence after power-on without pressing pairing:

- `[BLE-RECONNECT] Accepting bonded directed advertisement ...`
- Existing-bond re-encryption succeeds.
- `[BLE-RECONNECT] HID descriptor classified ... as BLE mouse ...`
- `bluepad32_platform: mouse ready`

Test power-cycle, adapter reboot with retained bonds, and sleep/wake. Also test an
MX keyboard and mouse separately, then Stadia/Xbox regression checks. The user confirmed the ERGO trackball reconnect fix works on 4 October 2026.
MX keyboard/mouse, adapter reboot and controller regression coverage remain to be
confirmed. This first fix handles exact identities observed here;
unresolved rotating private addresses are not bypassed. Multiple HID services
must each have the same supported class. Broader reconnect/pairing policy remains
future work; Settings OFF still stops scans.

Host-side classification tests:
`cc -Wall -Wextra -Werror -Isrc tests/ble_hid_appearance_test.c -o /tmp/ultrausbt-ble-hid-test`
then `/tmp/ultrausbt-ble-hid-test`.

## After the successful hardware test

Advertisement diagnostics are disabled by default. Normal boot, connection,
reconnection and error messages remain enabled. Continuous boot scanning remains
enabled for now; turning off diagnostics does not restore the pairing timeout.
Prepare an upstream Bluepad32 PR separately after reviewing portability and tests.

## Display names (4.2.2)

Successful BLE HID setup saves the name shown at discovery in the existing BTstack
TLV flash store (private `UBN` tag namespace). Each record contains a version,
bond identity address/type and a 31-character name matching the display storage.
The resolved security-manager bond index locates the record; the full identity is
validated before use, so a recycled bond slot cannot supply another device's name.
Unchanged records are not rewritten. Clear BT pair also clears these name records.
No pairing keys are included in this display metadata.

On reconnect, the saved name wins over the Device Information model number. A
bond created before 4.2.2 has no saved advertised name: it can display the model
number immediately, and re-pairing once saves the exact advertised name. No need
to clear all bonds. Verify the name after peripheral power-cycle and adapter
restart, and check that Clear BT pair still removes the associated metadata.
