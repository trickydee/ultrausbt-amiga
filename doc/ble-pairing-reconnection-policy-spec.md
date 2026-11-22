# Bluetooth pairing and reconnection policy

Status: proposed follow-up; not implemented in v4.3.0. Written 4 October 2026.

## Release boundary

The agreed v4.3.0 policy is to start pairing/scanning at boot and leave it enabled.
Settings → Pair OFF stops scanning and therefore prevents BLE reconnections as
well as new-device discovery. Existing connections remain active. Pair ON enables
both again. Advertisement debug logging stays off.

This spec separates those permissions in a later change. It must not hold up the
validated Logitech directed-advertisement fix or introduce a Bluepad32 dependency
upgrade into the 4.3.0 release. See [investigation notes](logitech-ble-investigation.md).

## Problem and goals

A BLE central needs to discover peripheral advertisements to reconnect. Stopping
scanning when the new-pairing timer expires prevents this. Conversely, continuing
unrestricted discovery admits unknown peripherals and can make several adapters
compete for the same device.

The adapter must allow a timed window for new pairings, permit authenticated
reconnection of existing bonds outside that window, and provide an explicit way
to stop accepting connections. A saved address is an admission hint, not proof of
identity; existing encryption keys must authenticate reconnecting peers.

## State model

Use one explicit policy enum rather than independently mutable booleans:

- OPEN: accept new supported HID devices and known-device reconnections. BLE scans
  run. A monotonic deadline controls permission to form new bonds.
- KNOWN_ONLY: scan for saved identities, reconnect using existing bonds, and reject
  unknown devices and attempts to establish replacement/new bonds.
- CLOSED: stop discovery and prevent new inbound/outbound connection attempts,
  including known-device reconnects. Do not disconnect already established links.

Proposed initial policy for the follow-up release: OPEN for 60 seconds at boot,
then KNOWN_ONLY. A user-selected CLOSED policy persists across reboot. OPEN is a
temporary action and is never persisted; selecting it resets the timer. Choosing
KNOWN_ONLY or CLOSED cancels it. Persist policy separately from bonds and USB role,
using versioned config migration. Invalid/old values map to the documented default,
never an accidental CLOSED or USB role change. Final UI/defaults require review
before implementation; 4.3.0 retains its explicitly agreed always-open startup.

The policy is not an ownership protocol. Two adapters with valid bonds and
reconnection enabled may still compete. Users must select CLOSED on adapters they
do not want accepting devices. Do not promise automatic handoff between hosts.

## Discovery and admission

1. Check the current policy before starting a connection, not merely when scans
   start. Late scan callbacks after CLOSED must not initiate connections.
2. Resolve advertiser identity using address type and saved BLE identity data.
   The exact-address match validated for ERGO M575B is the first path. Rotating
   resolvable private addresses require BTstack/SM resolution against saved IRKs;
   never infer identity from the name, manufacturer bytes or address prefix.
3. Resolve asynchronously with bounded pending work, deduplication and timeouts.
   Do not implement blocking AES/resolution loops in radio callbacks. Verify the
   precise APIs and controller capabilities in the pinned BTstack version first.
4. Apply admission to all connection paths: ordinary advertisements, directed
   advertisements, existing autoconnect paths and applicable Classic incoming links.
5. In KNOWN_ONLY, only resolved/exact bonded identities are eligible. Retain
   allowlist rules, resource limits, per-device retry backoff and HID validation.
6. Missing appearance/name on a known directed advertisement must not prevent
   admission. Recover the class from the HID descriptor and the display name from
   saved metadata/model information. Reject ambiguous classes safely.
7. Leave active scanning and scan duty-cycle tuning outside the initial policy
   change unless evidence requires them. Ensure failed connects resume scanning
   only when the current policy permits it.

## Authentication and pairing

Known-only admission must be followed by successful authentication with the saved
bond. Audit Security Manager callbacks and automatic GATT pairing behavior as well
as discovery. In particular, the existing missing-bond recovery can delete a bond
and request fresh pairing: it must not do so in KNOWN_ONLY or CLOSED.

When authentication fails outside OPEN: disconnect, retain useful diagnostic
state, avoid a tight reconnect loop, and tell the user to open pairing explicitly
if recovery needs a new bond. Do not automatically erase unrelated bonds.

Policy changes and connection setup can race. Re-check policy before accepting
pairing/security requests. On timeout, allow existing encrypted links and ongoing
known-bond reconnections to continue. For an unknown device still negotiating new
pairing at expiry, use a defined bounded grace token granted only while OPEN (for
example 15 seconds), or cancel cleanly. Select and document one policy before
coding. CLOSED cancels all unestablished attempts; stale callbacks must not reopen
scans or complete newly authorised pairing.

## OLED and API

Retain the Settings carousel and default Back selection. Proposed concise actions:

- Pair new: enter OPEN and show its remaining countdown.
- Reconnect ON: enter KNOWN_ONLY, closing the new-pairing window.
- Connections OFF: enter CLOSED, leaving current links working.
- Clear BT pair: retain confirmation; delete bonds and associated name metadata.

Show the active state explicitly (Pair / Known / Closed), rather than a single
Pair ON/OFF label that conflates permissions. Define clear-pair behavior: remain
in the current policy; KNOWN_ONLY with no bonds accepts nothing until Pair new is
selected. The existing quick shortcut should open/cancel the pairing window;
cancelling returns to KNOWN_ONLY, not CLOSED. Users use Settings for CLOSED.

Centralise transitions in one firmware API and route OLED/shortcut changes through
it. The policy and security actions execute in the BTstack context; use supported
safe dispatch when called elsewhere. Reverse USB Host Mode still does not start
Bluetooth and must not expose inapplicable settings.

## Classic Bluetooth boundary

Stopping inquiry is not equivalent to refusing inbound Classic connections.
Inventory Classic reconnect/link-key requests and apply the corresponding policy
there, or explicitly label a first implementation BLE-only. Do not ship a global
Connections OFF label while Classic can still accept new links. Test DualSense
and other existing Classic controllers as well as BLE devices.

## Logging and resource constraints

Keep functional connected/disconnected and concise policy-transition logs. Put
advertisement/identity tracing behind debug flags; never print LTKs or IRKs.
Distinguish unknown identity, policy denial and authentication failure. Rate-limit
retries and diagnostics. No changes to Amiga timing, Core 1 scheduling or GPIOs are
part of this work. Do not write settings flash on each advertisement or reconnect.

## Acceptance tests

- Pair a new ERGO/MX device during OPEN; after timeout reconnect it without its
  pairing button. Verify keys are reused and name/input remain correct.
- An unknown discoverable HID cannot connect or bond in KNOWN_ONLY.
- A peer with a known/spoofed address but without valid keys cannot gain access or
  replace a bond outside OPEN.
- Repeat peripheral off/on, sleep/wake and adapter reboot; test resolved private
  addresses separately from exact static identities.
- CLOSED blocks new connections, including reconnects, but preserves current
  input; reboot preserves a deliberately selected CLOSED state.
- OPEN can be re-entered from either state; timer extension and expiry are correct.
- Exercise timeout and manual CLOSED during discovery, identity resolution,
  encryption and HID enumeration. No stuck slots, orphan pauses or scan restarts.
- Test two adapters plus a Mac: only the adapter permitting connections should
  initiate/accept them; no claim of automatic ownership arbitration.
- Regress Stadia, Xbox, Classic controllers, USB input, mouse quadrature and CD32.
- Verify Clear BT pair, config migration, depleted device slots and retry backoff.

## Implementation sequence

1. Audit discovery/security/Classic entry points and write policy unit tests.
2. Introduce the explicit state machine and exact-bond-only admission.
3. Gate security recovery/new pairing and cover transition races.
4. Add bounded private-address resolution and test real rotating-address devices.
5. Add persistence and OLED actions, then run the hardware acceptance list.

Keep upstream directed reconnect support separate from this adapter UX/policy.
