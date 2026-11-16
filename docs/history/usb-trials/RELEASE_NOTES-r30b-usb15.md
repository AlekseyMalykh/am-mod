# Am MOD r30b-usb15

- Consistent FX shortcuts: Shift+B2 Paste, Shift+B3 Chain Bypass, Shift+Erase Copy. EQ adds a point with Shift+DATA; Sample Comp editor uses Shift+DATA for Warm.
- Stock firmware icons for the three added Project items and Mixer FX categories; original Project icons retained. Images are decoded from the pinned firmware mapping.
- Compressor pad lights use occupancy and a single selected cursor, avoiding whole-grid flashes from stock colour broadcasts.
- OptoCat DATA turn edits, click advances through all six parameters, Shift+click goes back. Physical knob layers remain available; Duck retains stock controls.
- Mixer entry can transition from the compressor through Sample.

Validated with host/sanitizers, AArch64/QEMU, native PNG/Image ABI, compressor UI/DSP, pad RGB, Mixer routing, all FX shortcuts, full preload and USB checks. Physical device testing is pending.

After copying: safely eject in Windows, B3/Return on MPC, then power off and on to load the new module.
