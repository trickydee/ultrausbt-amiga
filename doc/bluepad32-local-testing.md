# Testing the shared Bluepad32 reconnect branch

## Reproducible default build — 2 November 2026

The committed `bluepad32` submodule now uses `https://github.com/trickydee/bluepad32.git`,
branch `codex/ble-reconnect-pr`, pinned to `f7e94489be6ae29064a98c24c090dc5a5917c177`.
Use `git submodule update --init --recursive`, then `./build-all.sh` without an
external-source override. The gitlink selects the exact fix; the branch metadata
alone does not pin a revision. The fork's `main` does not contain this fix.
Native reconnect is used for both the default and optional external checkout;
the older build-tree patch is no longer applied.

Follow-up: return to upstream after PR #236 is merged and its replacement revision
has been validated on the adapters. Until then, retain this exact pin.

The remaining notes record earlier investigation builds and their inputs.


> Status update (2 November 2026): the upstream reconnect fix was submitted as
> [Bluepad32 PR #236](https://github.com/ricardoquesada/bluepad32/pull/236).
> The dated investigation and test notes below describe earlier stages; references
> to an unpublished PR or pending tests apply to those stages. Amiga and Apple ADB
> hardware tests succeeded, and the Atari MegaSTE upstream-versus-patched comparison
> confirmed reconnects with existing bonds. Jaguar hardware validation remains unconfirmed.

Firmware: **4.3.0**, Pico 2 W, PCB revision **5**.

The shared checkout is `/Users/rich/Documents/Code/Pico/bluepad32`, branch
`codex/bonded-ble-reconnect`, based on upstream develop at `4f27cea`.
The reconnect changes are committed locally as
`28ac7526840f32c7ec8cb63bff8c6281d49ad498`; no PR has been opened.
The user reported successful Amiga 4.3.0 hardware testing on 5 October 2026.
Sibling adapter validation is tracked in [cross-adapter testing](bluepad32-cross-adapter-testing.md).

Build from the adapter repository with the existing script:

```sh
BLUEPAD32_SOURCE_DIR=/Users/rich/Documents/Code/Pico/bluepad32 ./build-all.sh
```

Output: `dist/amigahid_pico2_w.uf2`. Verify the banner says **4.3.0 (PCB rev 5)**.
Bluepad32 itself reports **5.0.0** on this develop checkout; that is its library
version, not the adapter firmware version. SDK BTstack remains **1.6.2**.
UART also prints `[BT] Shared Bluepad32 checkout: native bonded reconnect test`.

The override is explicit for each build. Without it, the script uses the pinned
submodule and the existing adapter reconnect patch. CMake also accepts
`-DBLUEPAD32_SOURCE_DIR=...`; use the script for consistent firmware builds.

The external checkout's native reconnect implementation is compiled directly.
No adapter reconnect patch is applied to it. A compatibility header maps newer
HIDS host API names to the SDK's existing HIDS client functions. The adapter
retains bond-associated display names through its connection callback.
No submodule revision or GPIO mapping is changed.

## Hardware checks

- Pair each Logitech device once, then power it off/on without re-pairing.
- Repeat after restarting the adapter; verify input and the Map Devices name.
- Test MX Keys Mini keyboard, MX Master 3S mouse and Ergo M575 for Business trackball.
- Confirm Stadia reconnect and simultaneous mouse/keyboard/controller input.
- Pair a previously unknown device to check normal discovery still works.
- Check Clear BT Pair removes bonds and saved display names.
- Scanning remains enabled after boot. Manual Pair OFF must prevent reconnections;
  turn scanning on again for reconnect tests.

Steam hardware testing is still needed. Saved names from previous firmware are
reused; pairing once on this build saves the advertised name for a new bond.
The upstream PR omits the adapter's private persistent-name storage.

## Current-upstream Atari A/B comparison (7 October 2026)

The original shared checkout and tested Amiga image remain unchanged. Separate
Bluepad32 worktrees prepare the MegaSTE bench comparison:

- Baseline: `/tmp/bluepad32-ab-upstream`, upstream develop
  `1fef05f5cd68a902475a52207009f287f6954579`.
- Patched: `/tmp/bluepad32-ab-patched`, branch
  `codex/bonded-ble-reconnect-current`, commit
  `ea3cbf86c3d22df25738e08992f6d9732bb0ba7b`.

The reconnect commit was cherry-picked onto that upstream revision. Its single
advertisement-parser conflict was resolved by retaining upstream VID/PID parsing
alongside the directed-advertisement bond check. New upstream custom GATT paths
remain intact. Core host tests passed 6/6 and posix_imgui passed 1/1; this does not
constitute Steam/Switch 2 or adapter hardware validation. Tests used external
BTstack `8a8b616e04b07612a273978131efad32a083b1ed`.

Atari builds use the same adapter source snapshot and its `build-all.sh`, including
its existing bond-storage fixes. Hardware comparison is pending. Test baseline
first: named pairing, working input, then peripheral off/on without re-pairing.
Repeat on the patched firmware, then verify adapter-reboot persistence. Keep
scanning enabled. Preserve UART captures and record which image produced them.
No upstream PR has been published.

### Hardware result — 8 October 2026

User reports baseline fails to reconnect and patched firmware works on MegaSTE.
Reviewed captures show baseline creates valid bonds for MX Keys Mini and Ergo
M575 for Business trackball, then a subsequent startup yields no reconnect.
Patched firmware resolves those same identities, completes two re-encryptions,
classifies keyboard/mouse descriptors, and reaches ready without new pairing
starts. This validates the integrated `ea3cbf8` fix against upstream `1fef05f`.
It does not establish Steam/Switch 2 or Jaguar hardware compatibility.
PR notes updated in the shared Bluepad32 checkout and isolated integration branch;
no push or PR performed.
