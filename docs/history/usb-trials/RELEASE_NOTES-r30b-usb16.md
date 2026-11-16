# Am MOD r30b-usb16

- Uniform, centered 24px Project icon cells for all seven rows; Mixer FX icons unchanged.
- OptoCat DATA selection stays within three columns. Shift selects the alternate parameter layer while held; Shift+click moves backwards within the columns.
- Hidden Sample releases DATA/click to Duck/Opto overlays.
- Shift+KNOB FX works from the compressor using the last selected pad; pending Mixer navigation completes before the first Sample paint.
- Mixer/FX knobs and fader honour native Parameter, Knob FX and Fader Pickup/Scaled/Jump settings with independent binding histories.
- Original waveform colour takes priority over mute/solo palette tint.
- Soft Clipper Drive displays its actual -40..+40dB gain range (native 1..10000%, unity 100%). Stored values and DSP are unchanged.

Validated with host/sanitizers, AArch64/QEMU, native image/DSP and scoped UI/input regressions, full preload and USB. Physical device testing is pending.

Safely eject in Windows, B3/Return on MPC, then power off and on after installing.
