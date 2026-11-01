# r26c — fixes and new features

Superseded by [r26d](RELEASE_NOTES-r26d.md): r26c also erased native filter/envelope curves. The correction suppresses only the waveform.

[Русский](RELEASE_NOTES-r26c.ru.md) · Changes since r26a, including r26b.

This is a device-test candidate. Pad colours have been confirmed on the MPC; the remaining changes await device confirmation before publication.

## Fixed

- **Mixed projects after startup:** automatic restoration now loads the complete saved project. Previously, sounds from project B could appear alongside sequences from project A, or with no sequences after starting from an empty project. Project search now uses the same full-project loading behaviour.
- **Pad Colour focus:** the encoder focus follows the colour control and hands back to the native controls when changing parameters or holding Shift.
- **Blue playback flashes:** Sample pads retain their assigned colours when their sequence events play, including pads in other banks.
- **Green looked like Cyan:** Green now uses a distinct green hue without a blue component. Existing colour assignments keep their IDs.
- **Button LED interference:** button LED messages no longer cause unrelated pad RGB updates in Steps.

## Added and changed

- **Pad Colour on K2 without Shift:** Sample → B1 → Mix. K1 remains Volume; Shift+K2 is Kit Volume. Encoder presses cycle through Volume → Pad Colour → Pan.
- **Spectrum across B3:** the selected pad's spectrum is available on Filter, Filter Env and EQ.
- **Spectrum-only Filter pages:** Filter and Filter Env replace the waveform, playhead and time ruler with a spectrum and frequency labels. The screen headers and native parameter knobs remain available. EQ keeps its response curve and spectrum.

## Verification

- ARM/QEMU checks and UBSan passed for project loading, pad colours, focus, spectrum drawing and EQ.
- Full emulator testing restored project B's three note events, sequence, tempo and mixer/track volumes over an empty startup project. Playback RGB retained the assigned colour at changing brightness.
- The author confirmed pad colours on a real MPC. The r26c waveform replacement has passed drawing checks and is awaiting device feedback.

Startup still chooses the newest saved XPJ in the SD Projects folder or one directory level below it. Compatibility remains MPC Sample firmware **1.3.0.12** and an **exFAT** SD card; FAT32 support is not included.
