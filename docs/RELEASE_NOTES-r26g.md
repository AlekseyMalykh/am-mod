# r26g candidate — Scaled/Pickup takeover

[Русский](RELEASE_NOTES-r26g.ru.md) · Addresses [issue #2](https://github.com/AlekseyMalykh/am-mod/issues/2), superseding r26f.

In Scaled takeover, a fast knob turn could leave the parameter short of its limit. The r26f queue merged a whole movement into its last position. Native Scaled uses the first position to initialise its movement history; receiving only the endpoint can leave the parameter unchanged. Pickup also needs direction changes to recognise a crossing.

r26g retains each knob's initial position, turning points and latest position. Only intermediate points within a monotonic movement are merged. Button/pad/encoder events and ownership changes remain boundaries; final knob focus stays in arrival order. Your takeover setting and the stock takeover algorithm are unchanged. This correction applies across parameter pages, including Warp.

## Verification

- Executed the actual firmware takeover processor (`0x15f5690`): direct Scaled sweep reached 0; the r26f queue retained 0.45. The corrected queue matches direct native processing in Scaled, Pickup and Jump, for both limits and reversing trajectories, all three knob event codes and five starting parameter values.
- ARM/QEMU and UBSan input regressions pass, including full/wrapped queues, action/context boundaries, 50,000 concurrent updates, naming and resampling routes. Full preload constructor validation installs all 50 callbacks.
- GUI/parameter/timer receivers are fixtures; the takeover calculations execute native firmware code. Physical potentiometers, the complete Warp UI and audio were not tested. Device confirmation is still needed.

Firmware **1.3.0.12**, exFAT and **experimental FAT32** (as in r26e). Replace the complete `autorun/` folder or use installer 1.0.2. No formatting or moving samples/projects is required. This candidate has not been published.
