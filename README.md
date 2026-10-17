# HidDriver 360 — kernel 17511 prototype

This fork adds support for **Xbox 360 RGH/JTAG, retail kernel 2.0.17511.0**.
Based on [EinTim23/HidDriver360](https://github.com/EinTim23/hiddriver360).
Tested on a Jasper console. This is a device-tested prototype, not a claim of
universal USB controller compatibility. Other kernels/devices are not newly
validated by this fork.

**[Türkçe durum ve yapılacaklar](STATUS-TR.md)**

Tested and confirmed working:
- PlayStation Classic USB controller (054C:0CDA).
- PS1/PS2 controllers through the tested USB adapter (0810:0003).
- Hori Fighting Stick mini 4, PS4-043(U/E): 0F0D:00EC transitions to 00ED;
  the 00ED FF/5D/01 interface uses explicit XInput packet decoding.
- Original DualShock 3 CECHZC2E over USB (basic gameplay input).

Hori diagnostic v22 recorded 9,838 valid packets, zero malformed packets and
zero USB input errors, with virtual XInput button/direction events.
PSC/PS2 shortcut: **Start + R1 + R2 → R3**. No additional Guide shortcut added.

Pending:
- **Rumble output support** for compatible controllers is not implemented.
- **Nintendo SNES controller via USB adapter**: to be investigated and added
  after an adapter is available; no support or hardware validation claimed yet.
- Reliable takeover of controllers already plugged in at boot remains unconfirmed.
- Physical R3 on the tested PS adapter needs follow-up. DualShock 3 PS/Guide
  testing awaits repair of the owner's intermittently working physical PS button.

Build the driver project and `diagnostic/diagnostic.vcxproj` with the Xbox 360
XDK and the VS2010 toolset (`Release Retail`, `Xbox 360`). The diagnostic is an
application, not a DashLaunch plugin. It expects a packaged driver named
`hiddriver-17511-sdk.xex` beside it when loading on demand. Driver loading/media
packaging and the appropriate RGH/JTAG environment are still required; simply
renaming an SDK output does not perform packaging. This commit publishes source,
not a new binary release. Keep a working plugin backup before testing.

---

## Upstream README (original capabilities and instructions)
The statements below describe upstream; they are not a new test matrix for17511.

# HidDriver 360
Experimental on-console HID driver focussing on providing third party, non xinput controller support to the xbox 360 without dongles.

## What works
- Supports both 17559 retail and 17489 devkit dashboards
- Reading controller input via USB
- Up to 4 concurrent controllers, in combination with original xbox 360 controllers or without them
- Controllers are registered inside xam and therefore fully recognized in the entire xbox 360 user interface, ring of light will update accordingly, no original controllers are needed etc
- The controller works in all tested games

## Current limitations
- no rumble support

## Supported controllers
- DualShock 4 (PS4 controller)
- DualShock 4 Wireless Adapter (CUH-ZWA1E)
- DualSense (PS5 controller)
- DualSense Edge
- DualShock 3
- Nintendo Switch Pro controller
- Any USB HID compliant controller thanks to built in mapping assistant :)

## How to use
1. load plugin (either at runtime or at boot via your launch.ini)
2. Connect controllers via USB
3. Controllers with a built in mapping will start working right away, for all other controllers that are USB HID compliant a mapping assistant will be started that guides you through the process of mapping your controller
4. enjoy :)

## How to build
1. Acquire the official Xbox 360 SDK using black magic
2. Install visual studio 2010 ultimate and visual studio 2019
3. Install the sdk using the "FULL" preset
4. Open the solution in visual studio 2019 and build
5. Hopefully enjoy :)

## Showcase
https://github.com/user-attachments/assets/f090e5f4-538d-457f-8189-2c5b98579984


## Attributions
- [EinTim23](https://github.com/EinTim23/) for Reverse engineering the xbox 360s HID and controller implementation and implementing this driver
- [localcc](https://github.com/localcc/) for providing help about low level USB related questions and beaming the idea of doing this into my head
- [Rapidjson](https://github.com/Tencent/rapidjson/) JSON library used to save/load user defined mappings
- [Lufa](https://github.com/abcminiuser/lufa) HID report descriptor parser implementation used in hiddriver360 is derived from their implementation
- [iMoD1998](https://github.com/iMoD1998) Creating the Detours library used in hiddriver360
