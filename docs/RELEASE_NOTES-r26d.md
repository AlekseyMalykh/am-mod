# r26d — fixes and new features

[Русский](RELEASE_NOTES-r26d.ru.md) · Changes since r26a, including r26b.

The author approved r26d for the repository on 1 November 2026 after the device trial. Verification scopes are listed below.

## Fixed

- **Filter/envelope curves disappearing:** fixed the r26c graph-clear regression. Only the waveform is suppressed; native curves remain visible.
- **Spectrum after silence:** capture stays active while a spectrum page is open, so the next note is still shown after an idle period.
- **Mixed projects after startup:** automatic restoration now loads the complete saved project. Previously, sounds from project B could appear alongside sequences from project A, or with no sequences after starting from an empty project. Project search now uses the same full-project loading behaviour.
- **Pad Colour focus:** the encoder focus follows the colour control and hands back to the native controls when changing parameters or holding Shift.
- **Blue playback flashes:** Sample pads retain their assigned colours when their sequence events play, including pads in other banks.
- **Green looked like Cyan:** Green now uses a distinct green hue without a blue component. Existing colour assignments keep their IDs.
- **Button LED interference:** button LED messages no longer cause unrelated pad RGB updates in Steps.

## Added and changed

- **Pad Colour on K2 without Shift:** Sample → B1 → Mix. K1 remains Volume; Shift+K2 is Kit Volume. Encoder presses cycle through Volume → Pad Colour → Pan.
- **Spectrum across B3:** the selected pad's spectrum is available on Filter, Filter Env and EQ.
- **Spectrum with native curves:** Filter and Filter Env suppress only waveform rasterisation. Native filter and envelope curves, headers and parameter knobs remain available, with a dim spectrum underneath. This fixes the curves disappearing in r26c. EQ keeps its response curve and spectrum.

## Verification

- ARM/QEMU checks and UBSan passed for project loading, pad colours, focus, spectrum drawing and EQ.
- Full emulator testing restored project B's three note events, sequence, tempo and mixer/track volumes over an empty startup project. Playback RGB retained the assigned colour at changing brightness.
- The author confirmed pad colours on a real MPC and approved r26d after the device trial. In the full emulator, the waveform is absent on Filter/Filter Env, native curves remain visible, and Trim retains its waveform.

Startup still chooses the newest saved XPJ in the SD Projects folder or one directory level below it. Compatibility remains MPC Sample firmware **1.3.0.12** and an **exFAT** SD card; FAT32 support is not included.
