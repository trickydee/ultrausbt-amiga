# Shared Bluepad32 reconnect: cross-adapter testing

> Status update (2 November 2026): the upstream reconnect fix was submitted as
> [Bluepad32 PR #236](https://github.com/ricardoquesada/bluepad32/pull/236).
> The dated investigation and test notes below describe earlier stages; references
> to an unpublished PR or pending tests apply to those stages. Amiga and Apple ADB
> hardware tests succeeded, and the Atari MegaSTE upstream-versus-patched comparison
> confirmed reconnects with existing bonds. Jaguar hardware validation remains unconfirmed.

Date: 5 October 2026. Status: all three sibling adapter builds ready for hardware testing.

## Common source and integration contract

- Shared checkout: `/Users/rich/Documents/Code/Pico/bluepad32`.
- Bluepad32 revision: `28ac7526840f32c7ec8cb63bff8c6281d49ad498`.
- Upstream develop base: `4f27cea99b4c7cb73569356b6339d23cf3087cc6`.
- Adapter feature branch: `feature/bluepad32-logi-mx-investigations`, starting from each repository's `main`.
- All firmware builds use the repository's `build-all.sh`.
- Shared checkout selected explicitly using `BLUEPAD32_SOURCE_DIR`.
- Keep existing board GPIO mappings and timing. Compatibility changes belong in adapters.
- Pairing/scanning remains enabled for testing; advertisement debug logging stays off.
- No upstream PR, adapter merge, release or push until separately approved.

The original Amiga 4.3.0 test image used these same source changes before they
were committed. The user reports that image works correctly. Sibling builds
require independent hardware testing; compilation is not hardware validation.

## Common hardware checks

1. Pair the Logitech MX Keys Mini, MX Master 3S and Ergo M575 for Business trackball,
   where supported by the adapter. Confirm the expected input works.
2. Turn each peripheral off/on without entering pairing mode. Confirm reconnection
   and usable input. Repeat several times.
3. Restart the adapter with bonds retained, then repeat peripheral power cycles.
4. Check simultaneous peripherals and device names where the adapter displays them.
5. Check initial pairing of an unknown device still works and clearing bonds permits
   clean re-pairing. Use only the adapter's existing clear-bond controls.
6. Exercise a previously working gamepad (such as Stadia), where supported, alongside
   the Logitech devices to look for input stalls or pairing regressions.
7. If the UI permits disabling scanning, verify connections already established
   remain usable, new reconnects stop, and reconnects resume when scanning is enabled.

Pair the peripherals to each adapter deliberately. Power other adapters off during
initial validation to avoid multiple bonded hosts competing. Known-only pairing
policy and rotating-private-address support are separate future work.
Steam Controller behaviour is not hardware-verified.

## Build results

### Atari Jaguar — ready for hardware testing

- Firmware **0.3.0**, Pico 2 W, existing Jaguar pin map (no revision selector).
- Artifact: `/Users/rich/Documents/Code/console/jaguar/ultrausbt-atari-jaguar/dist/jaguar_pico2_w.uf2`.
- SHA-256: `c8b995a32e0bcdbed928d5100c3c3ca44a8aaf9740de413cd04e8c25c8211cd6`.
- Build, native reconnect marker, source path, hardware defaults and whitespace verified.
- Automatic timeout disabled for this build; explicit manual Pair OFF still works.

### Atari Mega / TT IKBD — ready for hardware testing

- Firmware **22.2.0-native-reconnect-test-dbg1**, Pico 2 W, existing debug/OLED variant.
- Artifact: `/Users/rich/Documents/Code/Pico/Atari-Keyboard/ultramegausb-atari-st-rpikbd/dist/atari_ikbd_pico2_w_debug.uf2`.
- SHA-256: `5a7857fef3bca5ca3e267ffc3fdc930295e0f402fb919225fb263746d4c3e139`.
- Build, native reconnect marker, source path, discovery callback and artifact validity verified.
- Existing local BTstack edits disable bond storage. They were preserved; this
  build uses a clean temporary copy of the same committed BTstack revision via
  `PICO_BTSTACK_PATH=/tmp/atari-reconnect-btstack`. Rebuild using the adapter's
  `docs/BLUEPAD32_NATIVE_RECONNECT_TEST.md` instructions, including that override.

### Apple ADB — ready for hardware testing

- Firmware **2.3.0**, Pico 2 W, main's existing unified adapter and SDK 2.2.0.
- Recommended UART-debug artifact: `/Users/rich/Documents/Code/Pico/Apple-ADB/ultramegausb-apple-adb/dist/ultrausbt-Apple-ADB-adapter-firmware-pico2_w-host-debug.uf2`.
- Debug SHA-256: `768c1bf92c5033d8094c812458412a4f2cf2fd095bd183e5759e646f3ed7209f`.
- Standard artifact: `/Users/rich/Documents/Code/Pico/Apple-ADB/ultramegausb-apple-adb/dist/ultrausbt-Apple-ADB-adapter-firmware-pico2_w-host.uf2`.
- Standard SHA-256: `659043203f7bfe3090452ad8173faced26597d6fc071b974844a2ed6a9f9f170`.
- Standard/debug adapter builds and the script's separate Pico adbmon build passed.
  adbmon is a passive diagnostic tool, not a Bluetooth adapter test image.
- Source flags, existing discovery admission, native marker, artifact hashes and
  whitespace checks verified. Existing unrelated release-note edits preserved.

All adapter edits remain uncommitted for review. Shared Bluepad32 is committed
locally; nothing was pushed and no PR was opened. See each adapter's test guide
for its exact build invocation and board-specific checks.
